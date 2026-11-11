/* BOURDONNEMENT - niveaux et objectif : composition de chaque niveau (monstres, pieges, noirceur), fusibles,
   faux fusibles, machoires, ascenseur, passage au niveau suivant, messages a l'ecran
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).

   Niveau 1 : la carte faite a la main (niveau_essai.c). Ensuite : generes par graine, plus grands, plus sombres,
   plus de pieges, et des monstres qui se melangent. Objectif : FUSIBLES_N fusibles -> courant -> ascenseur. */

static int niveauNum = 1;
static float niveauFog = FOG_DENSITE;
static float niveauT;                                     /* temps passe dans le niveau (hors cartons) */
static int radioDite;                                     /* bits : messages radio deja dits */
static void monde_reconstruire(void);                     /* rendu/monde.c : lightmaps et listes du nouveau niveau */
static void peur_reset(void);                             /* jeu/screamers.c */

/* message au centre bas de l'ecran (sous-titre) */
static char msgTexte[160];
static float msgT;
static void message(const char *s, float duree) { snprintf(msgTexte, sizeof msgTexte, "%s", s); msgT = duree; }

static float sortieT = -1;                                /* >= 0 : dans la cabine, portes qui se ferment */
static int fusPris;

/* qui hante le niveau n : N1 Rodeur, N2 Locataire, N3 Rampante, N4 Rodeur + Rampante, N5 Rodeur + Locataire,
   ensuite les trois */
static int niveau_monstres(int n, int types[3]) {
    switch (n) {
    case 1: types[0] = MON_RODEUR; return 1;
    case 2: types[0] = MON_LOCATAIRE; return 1;
    case 3: types[0] = MON_RAMPANTE; return 1;
    case 4: types[0] = MON_RODEUR; types[1] = MON_RAMPANTE; return 2;
    case 5: types[0] = MON_RODEUR; types[1] = MON_LOCATAIRE; return 2;
    default: types[0] = MON_RODEUR; types[1] = MON_LOCATAIRE; types[2] = MON_RAMPANTE; return 3;
    }
}

static void niveau_charger_ex(int n, int avecCarton) {
    niveauNum = n;
    if (n <= 1) {
        niveauMortsPM = 400; niveauFousPM = 80;
        charger_niveau_essai();
        niveauFog = FOG_DENSITE;
    } else {
        int k = n - 2;
        niveauMortsPM = 480 + 40 * k < 650 ? 480 + 40 * k : 650;
        niveauFousPM = 80;
        genEau = n == 3 ? 16 : 3 + k;                     /* le sous-sol humide est inonde */
        genVerre = 3 + k; genMachoires = 2 + k; genAlarmes = 1 + k / 2;
        int taille = CARTE_GEN_MIN + 6 * k < 84 ? CARTE_GEN_MIN + 6 * k : 84;
        generer_niveau(1000u + (unsigned)n * 7919u, taille, taille);
        niveauFog = FOG_DENSITE * (1.0f + 0.1f * (k + 1));
    }
    player_reset();
    monstres_reset();
    int types[3], nt = niveau_monstres(n, types);
    for (int i = 0; i < nt; i++) mon_ajouter(types[i], 1.0f + RODEUR_NIVEAU_K * (n - 1));
    peur_reset();
    fusPris = 0; sortieT = -1; niveauT = 0; radioDite = 0;
    msgT = 0;
    if (avecCarton) carton_niveau(n);
    monde_reconstruire();
}
static void niveau_charger(int n) { niveau_charger_ex(n, 1); }

static void dire_radio(int k) {
    if (radioDite & (1 << k)) return;
    radioDite |= 1 << k;
    message(radio(niveauNum, k), 7.5f);
    play(S_RADIO, 0.55f, 1.0f);
}

/* un objet a ramasser devant soi ? (fusible : 0..nfus-1 ; faux fusible : 100 + indice du piege) */
static int objet_vise(void) {
    V3 f = view_dir(P.yaw, 0);
    int best = -1; float bd = RAMASSER_DIST;
    for (int i = 0; i < nfus + npieges; i++) {
        V3 p;
        if (i < nfus) { if (fus[i].pris) continue; p = fus[i].pos; }
        else { Piege *pg = &pieges[i - nfus]; if (pg->type != PIEGE_ALARME || !pg->arme) continue; p = pg->pos; }
        V3 d = vsub(p, P.pos); d.y = 0;
        float l = hlen(d);
        if (l < bd && (l < 0.8f || (d.x * f.x + d.z * f.z) / l > 0.3f)) { bd = l; best = i < nfus ? i : 100 + i - nfus; }
    }
    return best;
}

static void objectif_tick(float dt) {
    if (msgT > 0) msgT -= dt;
    niveauT += dt;
    if (P.dead) return;
    if (niveauT > 3.5f) dire_radio(0);

    for (int i = 0; i < npieges; i++) {                   /* pieges */
        Piege *pg = &pieges[i];
        if (pg->t >= 0) pg->t += dt;
        if (pg->type == PIEGE_MACHOIRE && pg->arme && dist_xz(pg->pos, P.pos) < 0.42f) {
            pg->arme = 0; pg->t = 0;
            P.piegeT = MACHOIRE_T;
            play(S_MACHOIRE, 1.2f, 1.0f); play(S_CRI, 0.9f, 0.9f);
            faire_bruit(BRUIT_MACHOIRE);
            sursauter(BPM_SURSAUT * 1.3f);
            message("PIEGE !  martele [E] pour t'en sortir", 3.0f);
        }
        if (pg->type == PIEGE_ALARME && pg->t >= 0 && pg->t < ALARME_T) {   /* le faux fusible sonne */
            if ((int)(pg->t * 3) != (int)((pg->t - dt) * 3)) play3d(S_ALARME, v3(pg->pos.x, 0.3f, pg->pos.z), 1.0f, 1.0f, 30);
            bruit_monde(pg->pos, BRUIT_ALARME);
        }
    }

    if (actP[A_UTILISER] && P.piegeT <= 0) {              /* ramasser */
        int o = objet_vise();
        if (o >= 100) {                                   /* un faux : il hurle */
            Piege *pg = &pieges[o - 100];
            pg->arme = 0; pg->t = 0;
            sursauter(BPM_SURSAUT);
            message("C'ETAIT UN LEURRE.  Eloigne-toi de l'alarme.", 4.0f);
        } else if (o >= 0) {
            fus[o].pris = 1; fusPris++;
            play(S_FUSIBLE, 0.7f, 1.0f);
            faire_bruit(BRUIT_RAMASSER);
            if (fusPris == 1) dire_radio(1);
            if (fusPris < nfus) {
                if (fusPris > 1) { char m[64]; snprintf(m, sizeof m, "FUSIBLE %d / %d", fusPris, nfus); message(m, 3.0f); }
            } else {
                asc.alimente = 1;
                dire_radio(2);
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
