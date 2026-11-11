/* BOURDONNEMENT - le Rodeur : grand, maigre, aveugle. Il n'entend que le bruit.
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).

   Il erre dans un anneau autour du joueur (il se balade "a cote", on l'entend passer derriere le mur), s'arrete
   pour ecouter (claquements), va voir le moindre bruit, et poursuit quand il t'entend de pres ou fort.
   Accroupi, tu peux passer a deux metres de lui. En courant, il t'entend a quinze. La course le distance... tant
   que l'endurance tient. S'il te touche : screamer. L'hote le simulera en reseau (etape 4). */

enum { R_ERRE, R_ECOUTE, R_ENQUETE, R_CHASSE };
typedef struct {
    int actif;
    V3 pos, prevPos, cible;
    float yaw, etatT, repathT, oubliT, pasDist, anim, vitK;
    int etat, entendu;
    int souffle;                                          /* voix en boucle de sa respiration */
} Rodeur;
static Rodeur R;
static short rodeurDist[CARTE_MAX][CARTE_MAX];            /* distances vers sa cible (il descend la pente) */
static int attrape;                                       /* le Rodeur vient de toucher le joueur */

static float dist_xz(V3 a, V3 b) { float dx = a.x - b.x, dz = a.z - b.z; return sqrtf(dx * dx + dz * dz); }

static void rodeur_viser(V3 c) {
    R.cible = c;
    carte_distances_dans(rodeurDist, (int)floorf(c.x), (int)floorf(c.z));
    R.repathT = 0.5f;
}

/* case libre au hasard dans l'anneau [rmin, rmax] autour de p (accessible a pied) */
static int case_autour(V3 p, float rmin, float rmax, V3 *out) {
    for (int essai = 0; essai < 60; essai++) {
        float a = frand() * 2 * PI, r = rmin + frand() * (rmax - rmin);
        int gx = (int)floorf(p.x + sinf(a) * r), gz = (int)floorf(p.z + cosf(a) * r);
        if (cell_mur(gx, gz) || (asc.actif && gx == asc.gx && gz == asc.gz)) continue;
        *out = v3(gx + 0.5f, 0, gz + 0.5f);
        return 1;
    }
    return 0;
}

static void rodeur_etat(int e, float duree) { R.etat = e; R.etatT = duree; }

static void rodeur_reset(float vitK) {
    memset(&R, 0, sizeof R);
    R.actif = 1; R.vitK = vitK;
    attrape = 0;
    carte_distances((int)carteDepart.x, (int)carteDepart.z);   /* loin du depart, mais accessible */
    int best = -1, bx = 0, bz = 0;
    for (int z = 0; z < carteH; z++)
        for (int x = 0; x < carteW; x++) {
            int d = distCase[z][x];
            if (d < 0 || (asc.actif && x == asc.gx && z == asc.gz)) continue;
            int s = d >= (int)RODEUR_DEPART_MIN ? 1000 + (int)(frand() * 1000) : d;   /* au hasard parmi les lointaines */
            if (s > best) { best = s; bx = x; bz = z; }
        }
    R.pos = R.prevPos = v3(bx + 0.5f, 0, bz + 0.5f);
    rodeur_etat(R_ERRE, 0);
    rodeur_viser(R.pos);
}

/* entend-il le joueur maintenant ? (bruit x portee, moitie derriere un mur) */
static int rodeur_entend(float d) {
    if (P.dead || P.bruit < 0.01f) return 0;
    float portee = RODEUR_OUIE * P.bruit;
    if (!visible_xz(R.pos.x, R.pos.z, P.pos.x, P.pos.z)) portee *= 0.5f;
    return d < portee;
}

static void rodeur_tick(float dt) {
    if (!R.actif) return;
    R.prevPos = R.pos;
    float d = dist_xz(R.pos, P.pos);
    R.entendu = rodeur_entend(d);
    if (R.entendu) {
        if (R.etat == R_CHASSE) { R.oubliT = 0; if ((R.repathT -= dt) <= 0) rodeur_viser(P.pos); }
        else if (d < 5.0f || P.bruit >= 0.9f) {           /* trop pres ou trop fort : il fonce */
            rodeur_etat(R_CHASSE, 0); R.oubliT = 0; rodeur_viser(P.pos);
            play3d(S_RODEUR_CLIC, R.pos, 1.0f, 0.8f, 14);
        } else if (R.etat != R_ENQUETE || dist_xz(R.cible, P.pos) > 3) {   /* il a entendu quelque chose, par la */
            float err = d * 0.12f, a = frand() * 2 * PI;
            V3 c = v3(P.pos.x + sinf(a) * err, 0, P.pos.z + cosf(a) * err);
            if (cell_mur((int)floorf(c.x), (int)floorf(c.z))) c = P.pos;
            rodeur_etat(R_ENQUETE, 0); rodeur_viser(c);
        }
    }
    if (!P.dead && d < RODEUR_PRISE) attrape = 1;

    float v = 0;
    switch (R.etat) {
    case R_ERRE: v = RODEUR_ERRE_V; break;
    case R_ENQUETE: v = RODEUR_ENQUETE_V; break;
    case R_CHASSE:
        v = RODEUR_CHASSE_V;
        if (!R.entendu && (R.oubliT += dt) > RODEUR_OUBLI) rodeur_etat(R_ENQUETE, 0);   /* perdu : il va voir ou tu etais */
        break;
    case R_ECOUTE:
        if ((R.etatT -= dt) <= 0) rodeur_etat(R_ERRE, 0);
        break;
    }
    v *= R.vitK;

    if (v > 0) {
        int gx = (int)floorf(R.pos.x), gz = (int)floorf(R.pos.z);
        V3 vers = R.cible;
        int dc = (gx >= 0 && gz >= 0 && gx < CARTE_MAX && gz < CARTE_MAX) ? rodeurDist[gz][gx] : -1;
        if (dc > 0) {                                     /* descendre la pente des distances, case par case */
            int best = dc, bx = gx, bz = gz;
            for (int k = 0; k < 4; k++) {
                int nx = gx + cotes[k][0], nz = gz + cotes[k][1];
                if (nx < 0 || nz < 0 || nx >= CARTE_MAX || nz >= CARTE_MAX) continue;
                int dn = rodeurDist[nz][nx];
                if (dn >= 0 && dn < best) { best = dn; bx = nx; bz = nz; }
            }
            vers = v3(bx + 0.5f, 0, bz + 0.5f);
            if (visible_xz(R.pos.x, R.pos.z, R.cible.x, R.cible.z) && dist_xz(R.pos, R.cible) < 6) vers = R.cible;
        }
        V3 dir = vsub(vers, R.pos); dir.y = 0;
        float l = hlen(dir);
        if (l < 0.05f && dc <= 0) {                       /* arrive */
            if (R.etat == R_CHASSE) rodeur_viser(P.pos);
            else {
                if (R.etat == R_ENQUETE || frand() < 0.5f) {   /* il s'arrete pour ecouter */
                    rodeur_etat(R_ECOUTE, 2.0f + frand() * 2.5f);
                    play3d(S_RODEUR_CLIC, R.pos, 0.8f, 0.9f + 0.2f * frand(), 12);
                } else {
                    V3 c;
                    if (case_autour(P.pos, RODEUR_ERRE_RMIN, RODEUR_ERRE_RMAX, &c)) rodeur_viser(c);
                    rodeur_etat(R_ERRE, 0);
                }
            }
        } else if (l > 0.001f) {
            float step = fminf(v * dt, l);
            R.pos = vadd(R.pos, vmul(dir, step / l));
            float ty = atan2f(dir.x, -dir.z), dy = ty - R.yaw;
            while (dy > PI) dy -= 2 * PI;
            while (dy < -PI) dy += 2 * PI;
            R.yaw += dy * fminf(1, 6 * dt);
        }
        carte_collision(&R.pos.x, &R.pos.z, 0.3f);
        if (R.etat == R_ERRE && (R.repathT -= dt) <= -8) {  /* il change parfois d'idee en route */
            V3 c;
            if (case_autour(P.pos, RODEUR_ERRE_RMIN, RODEUR_ERRE_RMAX, &c)) rodeur_viser(c);
        }
    }
    float moved = hlen(vsub(R.pos, R.prevPos));
    R.anim += moved * 2.2f;
    R.pasDist += moved;
    if (R.pasDist > 0.95f) { R.pasDist = 0; play3d(S_RODEUR_PAS, R.pos, 1.0f, 0.85f + 0.2f * frand(), 15); }
}
