/* BOURDONNEMENT - meubles et pieces a theme : les donnees (types, emprises, collisions)
   Morceau de main.c : inclus dans l'ordre (AVANT monde/carte.c, qui s'en sert), ne se compile pas seul.

   Une piece = un rectangle creuse dans les murs, avec une seule porte, une zone (chambre, salle de bain...) qui
   choisit les materiaux, et des meubles poses selon un gabarit (monde/pieces_gen.c). Un meuble = un type, une
   position, une orientation (yaw multiple de 90 degres) et un etat (tele allumee, porte du frigo, mannequin...).
   Les meubles bloquants arretent joueurs et monstres (carte_collision) ; on peut ramper sous un lit. */

#define CARTE_MAX 96                                      /* cote maximal d'une carte (cases) */

enum { ZONE_BUREAU, ZONE_CHAMBRE, ZONE_SDB, ZONE_ENFANT, ZONE_CUISINE, ZONE_RESERVE, ZONE_NOE, ZONE_N };
static unsigned char zone[CARTE_MAX][CARTE_MAX];      /* zone de chaque case (sol, murs, ambiance) */
static unsigned char occupe[CARTE_MAX][CARTE_MAX];    /* case encombree par un meuble (chemins des monstres) */

enum { MB_LIT, MB_ARMOIRE, MB_CHEVET, MB_LAMPE, MB_TELE, MB_CHAISE, MB_TABLE, MB_LAVABO, MB_BAIGNOIRE, MB_TOILETTES,
       MB_PETIT_LIT, MB_JOUETS, MB_BOITE_MUSIQUE, MB_FRIGO, MB_COMPTOIR, MB_RAYONNAGE, MB_CARTONS, MB_MANNEQUIN,
       MB_SAC_COUCHAGE, MB_LANTERNE, MB_CASSETTE, MB_TYPES };
typedef struct { float hx, hz, h; int bloque, cachette; } MeubleDef;   /* demi-largeur (x local), demi-profondeur (z local) */
static const MeubleDef meubleDef[MB_TYPES] = {
    /* LIT          */ { 0.70f, 1.00f, 0.55f, 1, 1 },
    /* ARMOIRE      */ { 0.50f, 0.30f, 2.00f, 1, 2 },
    /* CHEVET       */ { 0.22f, 0.20f, 0.50f, 1, 0 },
    /* LAMPE        */ { 0.10f, 0.10f, 0.45f, 0, 0 },   /* posee sur un chevet */
    /* TELE         */ { 0.40f, 0.25f, 0.95f, 1, 0 },
    /* CHAISE       */ { 0.22f, 0.22f, 0.90f, 1, 0 },
    /* TABLE        */ { 0.55f, 0.40f, 0.75f, 1, 0 },
    /* LAVABO       */ { 0.30f, 0.22f, 0.85f, 1, 0 },
    /* BAIGNOIRE    */ { 0.40f, 0.85f, 0.55f, 1, 0 },
    /* TOILETTES    */ { 0.20f, 0.30f, 0.75f, 1, 0 },
    /* PETIT_LIT    */ { 0.45f, 0.70f, 0.45f, 1, 1 },
    /* JOUETS       */ { 0.25f, 0.25f, 0.20f, 0, 0 },
    /* BOITE_MUSIQUE*/ { 0.10f, 0.08f, 0.10f, 0, 0 },
    /* FRIGO        */ { 0.35f, 0.33f, 1.80f, 1, 0 },
    /* COMPTOIR     */ { 0.50f, 0.30f, 0.90f, 1, 0 },
    /* RAYONNAGE    */ { 0.50f, 0.25f, 2.00f, 1, 0 },
    /* CARTONS      */ { 0.30f, 0.30f, 0.60f, 1, 0 },
    /* MANNEQUIN    */ { 0.20f, 0.20f, 1.80f, 1, 0 },
    /* SAC_COUCHAGE */ { 0.40f, 0.95f, 0.15f, 0, 0 },
    /* LANTERNE     */ { 0.08f, 0.08f, 0.30f, 0, 0 },
    /* CASSETTE     */ { 0.10f, 0.06f, 0.04f, 0, 0 },
};

typedef struct {
    int type, piece;
    float x, z, yaw;                                      /* centre au sol ; yaw : 0 = le devant regarde vers -z */
    int etat;                                             /* tele : 0 eteinte 1 neige 2 visage ; frigo / armoire : ouvert... */
    int lumiere;                                          /* lampe, lanterne : indice de son neon (-1 : aucun) */
    float t;                                              /* temps depuis le dernier changement d'etat */
    float px[3], pz[3], pyaw[3];                          /* mannequin : ses places possibles ; etat = place actuelle */
} Meuble;
#define MAXMEUBLES 384
static Meuble meubles[MAXMEUBLES];
static int nmeubles;

typedef struct { int zone, x0, z0, iw, ih, porteX, porteZ, coteFond, vue; float t; } Piece;   /* interieur [x0, x0+iw[ x [z0, z0+ih[ */
#define MAXPIECES 12
static Piece pieces[MAXPIECES];
static int npieces;

static void meubles_vider(void) {
    nmeubles = 0; npieces = 0;
    memset(zone, 0, sizeof zone);
    memset(occupe, 0, sizeof occupe);
}

/* emprise au sol (demi-tailles dans le monde) : la rotation de 90 degres echange x et z */
static void meuble_emprise(const Meuble *m, float *hx, float *hz) {
    const MeubleDef *d = &meubleDef[m->type];
    int quart = ((int)floorf(m->yaw / (PI / 2) + 0.5f)) & 1;
    *hx = quart ? d->hz : d->hx; *hz = quart ? d->hx : d->hz;
}

static int add_meuble(int type, float x, float z, float yaw, int piece) {
    if (nmeubles >= MAXMEUBLES) return -1;
    Meuble *m = &meubles[nmeubles];
    memset(m, 0, sizeof *m);
    m->type = type; m->x = x; m->z = z; m->yaw = yaw; m->piece = piece; m->lumiere = -1;
    if (meubleDef[type].bloque && type != MB_MANNEQUIN) {   /* cases dont le centre est sous le meuble : encombrees */
        float hx, hz; meuble_emprise(m, &hx, &hz);
        for (int gz = (int)floorf(z - hz); gz <= (int)floorf(z + hz); gz++)
            for (int gx = (int)floorf(x - hx); gx <= (int)floorf(x + hx); gx++)
                if (gx >= 0 && gz >= 0 && gx < CARTE_MAX && gz < CARTE_MAX && fabsf(gx + 0.5f - x) < hx && fabsf(gz + 0.5f - z) < hz)
                    occupe[gz][gx] = 1;
    }
    return nmeubles++;
}

/* le point (x, z) est-il sous ce meuble ? (marge en plus ou en moins) */
static int dans_meuble(const Meuble *m, float x, float z, float marge) {
    float hx, hz; meuble_emprise(m, &hx, &hz);
    return fabsf(x - m->x) < hx + marge && fabsf(z - m->z) < hz + marge;
}

/* sous un lit ? (cachette : on y rampe accroupi) ; renvoie l'indice du lit ou -1 */
static int sous_un_lit(float x, float z) {
    for (int i = 0; i < nmeubles; i++)
        if (meubleDef[meubles[i].type].cachette == 1 && dans_meuble(&meubles[i], x, z, -0.05f)) return i;
    return -1;
}

/* repousse un cercle hors des meubles bloquants ; accroupi : les lits ne bloquent pas (on passe dessous) */
static void meubles_collision(float *x, float *z, float r, int accroupi) {
    for (int i = 0; i < nmeubles; i++) {
        const Meuble *m = &meubles[i];
        if (!meubleDef[m->type].bloque) continue;
        if (accroupi && meubleDef[m->type].cachette == 1) continue;
        if (fabsf(*x - m->x) > 2.5f || fabsf(*z - m->z) > 2.5f) continue;
        float hx, hz; meuble_emprise(m, &hx, &hz);
        float x0 = m->x - hx, x1 = m->x + hx, z0 = m->z - hz, z1 = m->z + hz;
        float cx = clampf(*x, x0, x1), cz = clampf(*z, z0, z1);
        float dx = *x - cx, dz = *z - cz, d2 = dx * dx + dz * dz;
        if (d2 >= r * r) continue;
        if (d2 > 1e-10f) { float d = sqrtf(d2); *x = cx + dx / d * r; *z = cz + dz / d * r; }
        else {                                            /* centre dedans : sortie par le cote le plus proche */
            float e[4] = { *x - x0, x1 - *x, *z - z0, z1 - *z };
            int k = 0;
            for (int j = 1; j < 4; j++) if (e[j] < e[k]) k = j;
            if (k == 0) *x = x0 - r; else if (k == 1) *x = x1 + r; else if (k == 2) *z = z0 - r; else *z = z1 + r;
        }
    }
}
