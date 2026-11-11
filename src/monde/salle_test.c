/* BOURDONNEMENT - salle de test de l'etape 0 : une piece jaune avec des piliers et des neons.
   Remplacee a l'etape 1 par la carte en boites (carte.c de VELOCITY) puis a l'etape 2 par la generation.
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

typedef struct { float x0, z0, x1, z1; } Pilier;          /* boite au sol, du sol au plafond */
static const Pilier piliers[] = {
    { -6.5f, -6.5f, -5.5f, -5.5f }, { 5.5f, -6.5f, 6.5f, -5.5f }, { -6.5f, 5.5f, -5.5f, 6.5f }, { 5.5f, 5.5f, 6.5f, 6.5f },
    { -1.0f, -3.0f, 3.5f, -2.6f },                         /* cloison basse au milieu */
    { -9.9f, 1.0f, -4.0f, 1.4f },                          /* demi-mur a gauche */
};
#define NPILIERS ((int)(sizeof piliers / sizeof piliers[0]))

/* neons du plafond : grille de 4 x 4 panneaux ; l'un d'eux clignote */
#define NEON_N 4
#define NEON_PAS (2 * SALLE_DEMI / NEON_N)
#define NEON_FOU 6                                        /* index du neon qui clignote */

/* clignotement deterministe : 1 = allume. Le motif varie, avec de longues periodes calmes. */
static int neon_allume(int i, float t) {
    if (i != NEON_FOU) return 1;
    float c = fmodf(t, 7.3f);
    if (c < 5.5f) return 1;                               /* calme */
    int k = (int)(c * 23.0f);
    return ((k * 2654435761u) >> 13) & 1;                 /* saute pendant 1,8 s */
}

/* repousse un cercle (x, z, r) hors des murs et des piliers */
static void salle_collision(float *x, float *z, float r) {
    float lim = SALLE_DEMI - r;
    *x = clampf(*x, -lim, lim); *z = clampf(*z, -lim, lim);
    for (int i = 0; i < NPILIERS; i++) {
        const Pilier *p = &piliers[i];
        float cx = clampf(*x, p->x0, p->x1), cz = clampf(*z, p->z0, p->z1);
        float dx = *x - cx, dz = *z - cz, d2 = dx * dx + dz * dz;
        if (d2 >= r * r) continue;
        if (d2 > 1e-8f) {                                 /* hors de la boite : on pousse le long de la normale */
            float d = sqrtf(d2);
            *x = cx + dx / d * r; *z = cz + dz / d * r;
        } else {                                          /* centre dans la boite : sortie par le cote le plus proche */
            float e[4] = { *x - p->x0, p->x1 - *x, *z - p->z0, p->z1 - *z };
            int m = 0;
            for (int k = 1; k < 4; k++) if (e[k] < e[m]) m = k;
            if (m == 0) *x = p->x0 - r; else if (m == 1) *x = p->x1 + r; else if (m == 2) *z = p->z0 - r; else *z = p->z1 + r;
        }
    }
}
