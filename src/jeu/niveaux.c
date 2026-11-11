/* BOURDONNEMENT - niveaux et objectif : fusibles, ascenseur, passage au niveau suivant, messages a l'ecran
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).

   Niveau 1 : la carte faite a la main (niveau_essai.c). Ensuite : generes par graine, plus grands, plus sombres,
   et le Rodeur va plus vite. Objectif de chaque niveau : FUSIBLES_N fusibles -> le courant revient -> l'ascenseur. */

static int niveauNum = 1;
static float niveauFog = FOG_DENSITE;
static void monde_reconstruire(void);                     /* rendu/monde.c : lightmaps et listes du nouveau niveau */
static void peur_reset(void);                             /* jeu/screamers.c */

static const char *niveau_nom(int n) {
    static const char *noms[] = { "LE HALL JAUNE", "LES BUREAUX MORTS", "LE SOUS-SOL HUMIDE", "LES COULOIRS SANS FIN" };
    static char buf[32];
    if (n >= 1 && n <= 4) return noms[n - 1];
    snprintf(buf, sizeof buf, "NIVEAU %d", n - 1);
    return buf;
}

/* message au centre bas de l'ecran (sous-titre) */
static char msgTexte[128];
static float msgT;
static void message(const char *s, float duree) { snprintf(msgTexte, sizeof msgTexte, "%s", s); msgT = duree; }

static float carteNiveauT;                                /* carton d'entree de niveau (secondes restantes) */
static float sortieT = -1;                                /* >= 0 : dans la cabine, portes qui se ferment */
static int fusPris;

static void niveau_charger(int n) {
    niveauNum = n;
    if (n <= 1) {
        niveauMortsPM = 260; niveauFousPM = 70;
        charger_niveau_essai();
        niveauFog = FOG_DENSITE;
    } else {
        int k = n - 2;
        niveauMortsPM = 340 + 60 * k < 600 ? 340 + 60 * k : 600;
        niveauFousPM = 70;
        int taille = CARTE_GEN_MIN + 6 * k < 84 ? CARTE_GEN_MIN + 6 * k : 84;
        generer_niveau(1000u + (unsigned)n * 7919u, taille, taille);
        niveauFog = FOG_DENSITE * (1.0f + 0.12f * (k + 1));
    }
    player_reset();
    rodeur_reset(1.0f + RODEUR_NIVEAU_K * (n - 1));
    peur_reset();
    fusPris = 0; sortieT = -1;
    carteNiveauT = 6.0f;
    message("Le courant de l'ascenseur est coupe. Trouve les fusibles. Ne fais pas de bruit.", 7.0f);
    monde_reconstruire();
}

static void objectif_tick(float dt) {
    if (msgT > 0) msgT -= dt;
    if (carteNiveauT > 0) carteNiveauT -= dt;
    if (P.dead) return;

    if (actP[A_UTILISER]) {                               /* ramasser un fusible devant soi */
        V3 f = view_dir(P.yaw, 0);
        int best = -1; float bd = RAMASSER_DIST;
        for (int i = 0; i < nfus; i++) {
            if (fus[i].pris) continue;
            V3 d = vsub(fus[i].pos, P.pos); d.y = 0;
            float l = hlen(d);
            if (l < bd && (l < 0.8f || (d.x * f.x + d.z * f.z) / l > 0.3f)) { bd = l; best = i; }
        }
        if (best >= 0) {
            fus[best].pris = 1; fusPris++;
            play(S_FUSIBLE, 0.7f, 1.0f);
            faire_bruit(BRUIT_RAMASSER);
            char m[64];
            if (fusPris < nfus) { snprintf(m, sizeof m, "FUSIBLE %d / %d", fusPris, nfus); message(m, 3.0f); }
            else {
                asc.alimente = 1;
                message("Le courant revient. Quelque part, l'ascenseur s'allume.", 5.0f);
                play3d(S_DING, v3(asc.gx + 0.5f, 1.5f, asc.gz + 0.5f), 1.2f, 1.0f, 30);
            }
        }
    }

    if (!asc.actif) return;
    V3 ac = v3(asc.gx + 0.5f, 0, asc.gz + 0.5f);
    float d = dist_xz(P.pos, ac);
    asc.dedans = (int)floorf(P.pos.x) == asc.gx && (int)floorf(P.pos.z) == asc.gz;
    if (sortieT < 0) {
        float voulu = asc.alimente && d < 3.2f ? 1.0f : 0.0f;   /* les portes s'ouvrent quand on approche */
        if (voulu > asc.ouverture && asc.ouverture == 0) play3d(S_PORTE, ac, 0.8f, 1.0f, 15);
        if (voulu > asc.ouverture) asc.ouverture = fminf(1, asc.ouverture + dt / ASC_PORTE_T);
        else if (voulu < asc.ouverture) asc.ouverture = fmaxf(0, asc.ouverture - dt / ASC_PORTE_T);
        if (asc.dedans && asc.alimente && d < 0.35f) {   /* au fond de la cabine : on part */
            sortieT = 0;
            play(S_PORTE, 0.8f, 0.9f);
        }
    } else {
        sortieT += dt;
        asc.ouverture = fmaxf(0, 1 - sortieT / ASC_PORTE_T);
        if (sortieT > ASC_PORTE_T && sortieT - dt <= ASC_PORTE_T) play(S_DING, 0.8f, 0.94f);
        if (sortieT > ASC_PORTE_T + 2.2f) niveau_charger(niveauNum + 1);
    }
}
