/* BOURDONNEMENT - le monde en grille : cases de 1 m (mur ou libre), neons du plafond, ascenseur, fusibles,
   collisions, rayons.
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).

   Pourquoi une grille et pas les boites de VELOCITY : les backrooms SONT une grille (dalles, neons alignes, murs
   d'un seul tenant du sol au plafond). La generation (generation.c) produit directement cette grille, et les ombres
   des neons se calculent vite en suivant les cases (ray_grille). Les objets (meubles) viendront en boites.

   Case (gx, gz) = x dans [gx, gx + 1], z dans [gz, gz + 1]. Sol en y = 0, plafond en y = HAUT_PLAFOND. */

#define CARTE_MAX 96
static unsigned char grille[CARTE_MAX][CARTE_MAX];        /* [gz][gx] : 1 = mur */
static unsigned char sansNeon[CARTE_MAX][CARTE_MAX];      /* jamais de neon ici (zone noire) */
static int carteW, carteH;
static V3 carteDepart;
static float carteDepartYaw;
static int niveauMortsPM = 120, niveauFousPM = 60;        /* pour mille : neons morts / fous (selon le niveau) */

enum { NEON_OK, NEON_FOU, NEON_MORT, NEON_ASC };          /* allume / clignote / eteint / cabine d'ascenseur */
typedef struct { float x, z; int etat; unsigned graine; } Neon;
#define MAXNEON 768
static Neon neons[MAXNEON];
static int nneons;

/* ascenseur : la cabine est une case libre fermee sur trois cotes ; la porte est sur le 4e (cote) */
typedef struct { int actif, gx, gz, cote; float ouverture; int alimente, dedans; } Ascenseur;   /* dedans : le joueur est dans la cabine */
static Ascenseur asc;
static const int cotes[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };   /* +x, -x, +z, -z */

/* sol : flaques (bruyantes, glissantes) et verre brise (craque) ; pieges : machoire de loup, faux fusible (alarme) */
enum { SOL_NORMAL, SOL_EAU, SOL_VERRE };
static unsigned char sol[CARTE_MAX][CARTE_MAX];
enum { PIEGE_MACHOIRE, PIEGE_ALARME };
typedef struct { V3 pos; int type, arme; float t; } Piege;  /* t : temps depuis le declenchement */
#define MAXPIEGE 48
static Piege pieges[MAXPIEGE];
static int npieges;
static float lumCase[CARTE_MAX][CARTE_MAX];               /* eclairement fixe du sol de chaque case (rendu/eclairage.c) */

#define MAXFUS 8
typedef struct { V3 pos; int pris; } Fusible;
static Fusible fus[MAXFUS];
static int nfus;

static int cell_mur(int gx, int gz) {
    if (gx < 0 || gz < 0 || gx >= carteW || gz >= carteH) return 1;
    return grille[gz][gx];
}
/* pour se deplacer : la cabine est fermee tant que les portes ne sont pas ouvertes */
static int cell_bloque(int gx, int gz) {
    if (asc.actif && gx == asc.gx && gz == asc.gz && asc.ouverture < 0.85f && !asc.dedans) return 1;
    return cell_mur(gx, gz);
}

static unsigned hash32(unsigned x) {                      /* melange d'entiers (choix deterministes par case) */
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}

static void add_neon(int gx, int gz, int etat) {
    if (nneons >= MAXNEON) return;
    Neon *n = &neons[nneons++];
    n->x = gx + 0.5f; n->z = gz + 0.5f; n->etat = etat;
    n->graine = hash32((unsigned)(gx * 73856093) ^ (unsigned)(gz * 19349663));
}

static void carte_vider(int w, int h) {
    memset(grille, 1, sizeof grille);
    memset(sansNeon, 0, sizeof sansNeon);
    carteW = w < CARTE_MAX ? w : CARTE_MAX; carteH = h < CARTE_MAX ? h : CARTE_MAX;
    nneons = 0; nfus = 0; npieges = 0;
    memset(sol, 0, sizeof sol);
    memset(lumCase, 0, sizeof lumCase);
    memset(&asc, 0, sizeof asc);
    carteDepart = v3(carteW * 0.5f, 0, carteH * 0.5f); carteDepartYaw = 0;
}

static void add_fusible(int gx, int gz) {
    if (nfus >= MAXFUS) return;
    fus[nfus].pos = v3(gx + 0.5f, 0, gz + 0.5f);
    fus[nfus].pris = 0;
    nfus++;
}

static void add_piege(int gx, int gz, int type) {
    if (npieges >= MAXPIEGE) return;
    pieges[npieges].pos = v3(gx + 0.5f, 0, gz + 0.5f);
    pieges[npieges].type = type; pieges[npieges].arme = 1; pieges[npieges].t = -1;
    npieges++;
}

static int sol_en(float x, float z) {
    int gx = (int)floorf(x), gz = (int)floorf(z);
    if (gx < 0 || gz < 0 || gx >= carteW || gz >= carteH) return SOL_NORMAL;
    return sol[gz][gx];
}

/* eclairement fixe pres d'un point (objets, monstres, le Locataire fige sous un neon) */
static float lum_case(float x, float z) {
    int gx = (int)floorf(x), gz = (int)floorf(z);
    if (gx < 0 || gz < 0 || gx >= carteW || gz >= carteH) return 0;
    return lumCase[gz][gx];
}

/* cabine en (gx, gz) : sa porte donne sur la seule case libre voisine */
static void poser_ascenseur(int gx, int gz) {
    asc.actif = 1; asc.gx = gx; asc.gz = gz; asc.cote = 0; asc.ouverture = 0; asc.alimente = 0;
    for (int k = 0; k < 4; k++) if (!cell_mur(gx + cotes[k][0], gz + cotes[k][1])) { asc.cote = k; break; }
    add_neon(gx, gz, NEON_ASC);
}

/* neons automatiques : une case libre sur NEON_PAS_CASES (x et z), sauf zones noires ; quelques morts ou fous */
static void placer_neons(void) {
    for (int z = 0; z < carteH; z++)
        for (int x = 0; x < carteW; x++) {
            if (grille[z][x] || sansNeon[z][x] || x % NEON_PAS_CASES != 1 || z % NEON_PAS_CASES != 1) continue;
            if (asc.actif && x == asc.gx && z == asc.gz) continue;
            int deja = 0;
            for (int i = 0; i < nneons; i++) if ((int)neons[i].x == x && (int)neons[i].z == z) deja = 1;
            if (deja) continue;
            unsigned h = hash32((unsigned)(x * 31 + z * 977) ^ 0x9e3779b9u ^ (unsigned)(niveauMortsPM * 7919)) % 1000;
            add_neon(x, z, h < (unsigned)niveauMortsPM ? NEON_MORT : h < (unsigned)(niveauMortsPM + niveauFousPM) ? NEON_FOU : NEON_OK);
        }
}

/* Carte en texte : '#' mur, '.' sol (neon automatique), ',' sol sans neon, 'n' neon force, 'f' neon qui clignote,
   'x' neon mort, 'P' depart du joueur (regarde vers -z), 'F' fusible, 'E' cabine d'ascenseur,
   '~' flaque, '%' verre brise, 'M' machoire de loup, 'A' faux fusible (alarme). */
static void carte_charger(const char *const *lignes, int n) {
    int w = 0;
    for (int z = 0; z < n; z++) { int l = (int)strlen(lignes[z]); if (l > w) w = l; }
    carte_vider(w, n);
    int ex = -1, ez = -1;
    for (int z = 0; z < carteH; z++) {
        int l = (int)strlen(lignes[z]);
        for (int x = 0; x < carteW; x++) {
            char c = x < l ? lignes[z][x] : '#';          /* ligne courte : completee par des murs */
            grille[z][x] = c == '#';
            if (c == ',') sansNeon[z][x] = 1;
            if (c == 'P') carteDepart = v3(x + 0.5f, 0, z + 0.5f);
            if (c == 'F') add_fusible(x, z);
            if (c == '~') sol[z][x] = SOL_EAU;
            if (c == '%') sol[z][x] = SOL_VERRE;
            if (c == 'M') add_piege(x, z, PIEGE_MACHOIRE);
            if (c == 'A') add_piege(x, z, PIEGE_ALARME);
            if (c == 'E') { ex = x; ez = z; }
            if (c == 'n') add_neon(x, z, NEON_OK);
            else if (c == 'f') add_neon(x, z, NEON_FOU);
            else if (c == 'x') add_neon(x, z, NEON_MORT);
        }
    }
    if (ex >= 0) poser_ascenseur(ex, ez);
    placer_neons();
}

/* etat d'un neon a l'instant t : 1 allume, 0 eteint. Un neon fou a de longues periodes calmes puis "saute" */
static int neon_allume(int i, float t) {
    const Neon *n = &neons[i];
    if (n->etat == NEON_OK) return 1;
    if (n->etat == NEON_MORT) return 0;
    if (n->etat == NEON_ASC) return asc.alimente;
    float periode = 5.0f + (n->graine % 400) * 0.01f, crise = 1.2f + (n->graine >> 9) % 100 * 0.01f;
    float c = fmodf(t + (n->graine >> 16) % 100 * 0.07f, periode);
    if (c < periode - crise) return 1;
    int k = (int)(c * 23.0f);
    return (hash32((unsigned)k ^ n->graine) >> 7) & 1;
}
static int neon_dynamique(int i) { return neons[i].etat == NEON_FOU || neons[i].etat == NEON_ASC; }

/* rayon dans la grille, plan XZ (Amanatides & Woo). (dx, dz) : direction ; t est mesure dans l'unite de cette
   direction (si elle vient d'un vecteur 3D norme, t = distance 3D). Renvoie le t du premier mur, ou maxt. */
static float ray_grille(float ox, float oz, float dx, float dz, float maxt) {
    int gx = (int)floorf(ox), gz = (int)floorf(oz);
    if (cell_mur(gx, gz)) return 0;
    int sx = dx > 0 ? 1 : -1, sz = dz > 0 ? 1 : -1;
    float adx = fabsf(dx), adz = fabsf(dz);
    float tdx = adx > 1e-9f ? 1.0f / adx : 1e30f, tdz = adz > 1e-9f ? 1.0f / adz : 1e30f;
    float tmx = adx > 1e-9f ? (dx > 0 ? gx + 1 - ox : ox - gx) * tdx : 1e30f;
    float tmz = adz > 1e-9f ? (dz > 0 ? gz + 1 - oz : oz - gz) * tdz : 1e30f;
    for (;;) {
        float t;
        if (tmx < tmz) { t = tmx; if (t > maxt) return maxt; gx += sx; tmx += tdx; }
        else { t = tmz; if (t > maxt) return maxt; gz += sz; tmz += tdz; }
        if (cell_mur(gx, gz)) return t;
    }
}

/* rayon 3D : murs de la grille, sol et plafond. d doit etre norme. */
static float ray_world(V3 o, V3 d, float maxt) {
    if (d.y < -1e-6f) maxt = fminf(maxt, (0 - o.y) / d.y);
    else if (d.y > 1e-6f) maxt = fminf(maxt, (HAUT_PLAFOND - o.y) / d.y);
    return ray_grille(o.x, o.z, d.x, d.z, fmaxf(maxt, 0));
}

/* aucun mur entre deux points (plan XZ) ? */
static int visible_xz(float ax, float az, float bx, float bz) {
    return ray_grille(ax, az, bx - ax, bz - az, 1.0f) >= 1.0f;
}

/* repousse un cercle (x, z, r) hors des cases bloquees voisines (deux passes pour les coins) */
static void carte_collision(float *x, float *z, float r) {
    for (int passe = 0; passe < 2; passe++) {
        int x0 = (int)floorf(*x - r) - 1, x1 = (int)floorf(*x + r) + 1, z0 = (int)floorf(*z - r) - 1, z1 = (int)floorf(*z + r) + 1;
        for (int gz = z0; gz <= z1; gz++)
            for (int gx = x0; gx <= x1; gx++) {
                if (!cell_bloque(gx, gz)) continue;
                float cx = clampf(*x, (float)gx, gx + 1.0f), cz = clampf(*z, (float)gz, gz + 1.0f);
                float dx = *x - cx, dz = *z - cz, d2 = dx * dx + dz * dz;
                if (d2 >= r * r) continue;
                if (d2 > 1e-10f) { float d = sqrtf(d2); *x = cx + dx / d * r; *z = cz + dz / d * r; }
                else {                                    /* centre dans le mur : sortie par le cote le plus proche */
                    float e[4] = { *x - gx, gx + 1 - *x, *z - gz, gz + 1 - *z };
                    int m = 0;
                    for (int k = 1; k < 4; k++) if (e[k] < e[m]) m = k;
                    if (m == 0) *x = gx - r; else if (m == 1) *x = gx + 1 + r; else if (m == 2) *z = gz - r; else *z = gz + 1 + r;
                }
            }
    }
}

/* distances en cases depuis (sx, sz) par les cases libres (parcours en largeur) ; -1 = inaccessible.
   Sert aux tests, a la generation (placer fusibles et ascenseur loin) et aux chemins du monstre. */
static short distCase[CARTE_MAX][CARTE_MAX];
static void carte_distances_dans(short d[CARTE_MAX][CARTE_MAX], int sx, int sz) {
    static short file[CARTE_MAX * CARTE_MAX][2];
    for (int z = 0; z < CARTE_MAX; z++) for (int x = 0; x < CARTE_MAX; x++) d[z][x] = -1;
    if (cell_mur(sx, sz)) return;
    int a = 0, b = 0;
    d[sz][sx] = 0; file[b][0] = (short)sx; file[b][1] = (short)sz; b++;
    while (a < b) {
        int x = file[a][0], z = file[a][1]; a++;
        for (int k = 0; k < 4; k++) {
            int nx = x + cotes[k][0], nz = z + cotes[k][1];
            if (cell_mur(nx, nz) || d[nz][nx] >= 0) continue;
            d[nz][nx] = (short)(d[z][x] + 1);
            file[b][0] = (short)nx; file[b][1] = (short)nz; b++;
        }
    }
}
static void carte_distances(int sx, int sz) { carte_distances_dans(distCase, sx, sz); }

/* nombre de cases libres atteignables depuis le depart */
static int carte_atteignables(int *libres) {
    int n = 0;
    *libres = 0;
    carte_distances((int)carteDepart.x, (int)carteDepart.z);
    for (int z = 0; z < carteH; z++)
        for (int x = 0; x < carteW; x++) { *libres += !grille[z][x]; n += distCase[z][x] >= 0; }
    return n;
}
