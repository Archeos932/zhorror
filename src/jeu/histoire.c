/* BOURDONNEMENT - l'histoire a l'ecran : cartons plein ecran tapes a la machine (ouverture, entree de chaque
   niveau) et messages radio pendant le jeu. Les textes sont dans textes.c (genere par outils/textes.py).
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   Pendant un carton, le monde est en pause. E, Espace ou clic : finir la ligne, puis continuer. */

#define CARTON_VITESSE 42.0f                              /* caracteres par seconde */
static const char *const *cartonL;
static int cartonActif, cartonSuite;                      /* cartonSuite : 1 = ouvrir ensuite le carton du niveau */
static float cartonCars, cartonT;                         /* caracteres deja tapes, temps */
static int cartonTotal;
static char cartonTitre[64];

static void carton_ouvrir(const char *const *lignes, const char *titre, int suite) {
    cartonL = lignes; cartonActif = 1; cartonSuite = suite;
    cartonCars = 0; cartonT = 0; cartonTotal = 0;
    for (int i = 0; lignes[i]; i++) cartonTotal += (int)strlen(lignes[i]) + 8;   /* + une pause par ligne */
    snprintf(cartonTitre, sizeof cartonTitre, "%s", titre ? titre : "");
}

static const char *const *texte_niveau(int n) {
    int i = n - 1 < NTEXTES_NIVEAUX - 1 ? n - 1 : NTEXTES_NIVEAUX - 1;
    return texteNiveaux[i < 0 ? 0 : i];
}
static const char *titre_niveau(int n) {
    static char buf[64];
    int i = n - 1 < NTEXTES_NIVEAUX - 1 ? n - 1 : NTEXTES_NIVEAUX - 1;
    if (i == NTEXTES_NIVEAUX - 1) snprintf(buf, sizeof buf, "NIVEAU %d", n - 1);   /* au-dela : plus de nom */
    else snprintf(buf, sizeof buf, "NIVEAU %d  -  %s", n - 1, titreNiveaux[i < 0 ? 0 : i]);
    return buf;
}
static void carton_niveau(int n) { carton_ouvrir(texte_niveau(n), titre_niveau(n), 0); }

/* un tick de carton ; renvoie 1 s'il est ouvert (le monde attend) */
static int carton_tick(float dt) {
    if (!cartonActif) return 0;
    cartonT += dt;
    int avant = (int)cartonCars;
    if (cartonCars < cartonTotal) cartonCars += CARTON_VITESSE * dt;
    if ((int)cartonCars / 3 != avant / 3 && cartonCars < cartonTotal) play(S_CLIC, 0.12f, 0.6f + 0.3f * frand());   /* machine */
    if (actP[A_UTILISER] || actP[A_CRIER] || actP[A_LAMPE]) {
        if (cartonCars < cartonTotal) cartonCars = (float)cartonTotal;
        else {
            cartonActif = 0;
            play(S_CLIC, 0.4f, 0.5f);
            if (cartonSuite) carton_ouvrir(texte_niveau(1), titre_niveau(1), 0);
        }
    }
    return 1;
}

/* message radio du niveau n (0 : arrivee, 1 : premier fusible, 2 : tous) */
static const char *radio(int n, int k) {
    int i = n - 1 < NTEXTES_RADIO - 1 ? n - 1 : NTEXTES_RADIO - 1;
    return texteRadio[i < 0 ? 0 : i][k];
}
