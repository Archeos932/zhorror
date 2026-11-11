/* BOURDONNEMENT - la peur : coeur (BPM), screamer de mort, apparitions dans le faisceau, images subliminales,
   faux bruits, lampe qui gresille quand un monstre est pres ; pilote la musique (son/ambiance.c).
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   Regle d'or : la peur vient de l'attente. Les screamers sont rares et jamais deux fois de suite. */

static float screamT = -1;                                /* >= 0 : screamer de mort en cours (secondes) */
static int screamType;                                    /* l'espece du tueur (tete du screamer, texte de mort) */
static float mortT = -1;                                  /* >= 0 : ecran de mort (secondes) */
static float apparitionT = -1;                            /* >= 0 : apparition en cours */
static V3 apparitionPos;
static int apparitionType;
static float apparitionProchaine, fauxBruitProchain, subliminalProchain;
static float subliminalT = -1;                            /* >= 0 : image subliminale (2 images) */
static float lampeCoupure;                                /* > 0 : la lampe est coupee */
static float tension;                                     /* 0..1 : proximite et poursuite */
static float bpm = BPM_REPOS, coeurPhase, coeurPouls;     /* coeurPouls : 1 au battement, retombe (ecran) */
static int coeurBat;                                      /* battements (tests) */

static void peur_reset(void) {
    screamT = -1; mortT = -1; apparitionT = -1; subliminalT = -1; lampeCoupure = 0; tension = 0;
    bpm = BPM_REPOS + 10; sursautDemande = 0;
    apparitionProchaine = APPARITION_MIN * 0.6f + frand() * (APPARITION_MAX - APPARITION_MIN) * 0.5f;
    fauxBruitProchain = FAUX_BRUIT_MIN + frand() * (FAUX_BRUIT_MAX - FAUX_BRUIT_MIN);
    subliminalProchain = SUBLIMINAL_MIN + frand() * (SUBLIMINAL_MAX - SUBLIMINAL_MIN);
}

/* intensite de la lampe a cet instant (0..1) : batterie faible, monstre proche, apparition */
static float lampe_intensite(float t) {
    if (!P.lampe || lampeCoupure > 0 || apparitionT >= 0.28f) return 0;
    float k = LAMPE_PUISSANCE;
    unsigned h = hash32((unsigned)(t * 30.0f));
    if (P.batterie < LAMPE_FAIBLE * LAMPE_BATTERIE && (h & 15) < 3) k *= 0.15f;
    float d;
    if (mon_proche(&d) >= 0 && d < LAMPE_MONSTRE_D) {     /* il est pres : le faisceau sursaute */
        float p = (1 - d / LAMPE_MONSTRE_D) * 0.6f;
        if ((h >> 4) % 100 < (unsigned)(p * 100)) k *= 0.1f + 0.3f * ((h >> 12) & 1);
    }
    return k;
}

static void mort_commencer(int type) {
    P.dead = 1; screamT = 0;
    screamType = type;
    play(S_HURLEMENT, 1.4f, 1.0f);                        /* trois hurlements empiles + l'impact */
    play(S_HURLEMENT, 1.0f, 0.71f);
    play(S_HURLEMENT, 0.7f, 1.41f);
    play(S_FAUX_BRUIT, 1.3f, 0.6f);
    play(S_STINGER, 0.9f, 0.8f);
    bpm = BPM_MAX;
}

static void peur_tick(float dt) {
    if (tueur >= 0 && !P.dead) mort_commencer(mon[tueur].type);
    if (subliminalT >= 0 && (subliminalT += dt) > 0.07f) subliminalT = -1;
    if (screamT >= 0) {
        screamT += dt;
        if (screamT > SCREAMER_MORT_T) { screamT = -1; mortT = 0; }
        return;
    }
    if (mortT >= 0) {
        mortT += dt;
        if (netMode == NET_OFF && mortT > 1.5f && (actP[A_UTILISER] || actP[A_CRIER])) niveau_charger_ex(niveauNum, 0);
        if (netMode != NET_OFF && mortT > 2.5f && actP[A_UTILISER]) spectateurK += 1;   /* en ligne : regarder un autre */
        return;
    }
    float d;
    int mp = mon_proche(&d);
    float proche = mp >= 0 ? clampf(1 - d / 18.0f, 0, 1) : 0;
    int chasse = 0;
    for (int i = 0; i < nmon; i++) if (mon[i].etat == E_CHASSE && (mon[i].type != MON_LOCATAIRE || dist_xz(mon[i].pos, P.pos) < 12)) chasse = 1;
    float voulu = fmaxf(proche * 0.8f, chasse ? 1.0f : 0.0f);
    if (P.piegeT > 0) voulu = fmaxf(voulu, 0.7f);
    tension += (voulu - tension) * fminf(1, dt * (voulu > tension ? 3.0f : 0.5f));

    /* coeur : cible selon l'effort et la peur, sursauts ; il monte vite et redescend lentement */
    float cible = BPM_REPOS + (P.court ? BPM_COURSE : 0) + (P.essouffle ? BPM_COURSE * 0.5f : 0) + BPM_TENSION * tension;
    if (sursautDemande > 0) { bpm = fminf(BPM_MAX, bpm + sursautDemande); sursautDemande = 0; }
    bpm += (cible - bpm) * fminf(1, dt * (cible > bpm ? 0.9f : 0.12f));
    bpm = clampf(bpm, BPM_REPOS - 6, BPM_MAX);
    coeurPhase += dt * bpm / 60.0f;
    coeurPouls = fmaxf(0, coeurPouls - dt * 5);
    if (coeurPhase >= 1) {
        coeurPhase -= 1; coeurBat++;
        coeurPouls = 1;
        amb_battement(clampf((bpm - 62) / 110.0f, 0.12f, 1.0f));
    }
    if (lampeCoupure > 0) lampeCoupure -= dt;

    if (apparitionT >= 0) {                               /* apparition : il est la, au bout du faisceau... */
        apparitionT += dt;
        if (apparitionT > APPARITION_T) { apparitionT = -1; lampeCoupure = 0.45f; }   /* ... puis plus rien */
    } else if ((apparitionProchaine -= dt) <= 0) {
        apparitionProchaine = 15;                         /* conditions pas reunies : on reessaie bientot */
        V3 f = view_dir(P.yaw, 0);
        float libre = ray_grille(P.pos.x, P.pos.z, f.x, f.z, 12);
        if (!chasse && d > 10 && P.lampe && libre > 5.5f && fabsf(P.pitch) < 0.5f && nmon > 0) {
            float at = fminf(libre - 1.0f, 5.0f + frand() * 3.0f);
            apparitionPos = v3(P.pos.x + f.x * at, 0, P.pos.z + f.z * at);
            apparitionType = mon[(int)(frand() * nmon) % nmon].type;
            apparitionT = 0;
            play(S_STINGER, 1.1f, 0.95f + 0.1f * frand());
            sursauter(BPM_SURSAUT);
            apparitionProchaine = APPARITION_MIN + frand() * (APPARITION_MAX - APPARITION_MIN);
            if (netMode != NET_CLIENT && mon[0].type == MON_RODEUR && frand() < 0.5f) { mon_etat(&mon[0], E_ENQUETE, 0); mon_viser(&mon[0], P.pos); }   /* et le vrai arrive */
        }
    }
    if (flashsOn && (subliminalProchain -= dt * (0.3f + tension)) <= 0) {   /* deux images du visage, a peine vues */
        subliminalProchain = SUBLIMINAL_MIN + frand() * (SUBLIMINAL_MAX - SUBLIMINAL_MIN);
        subliminalT = 0;
        play(S_STINGER, 0.35f, 1.6f);
        sursauter(BPM_SURSAUT * 0.4f);
    }
    if ((fauxBruitProchain -= dt) <= 0) {                 /* un choc lourd derriere toi, quelque part */
        fauxBruitProchain = FAUX_BRUIT_MIN + frand() * (FAUX_BRUIT_MAX - FAUX_BRUIT_MIN);
        V3 f = view_dir(P.yaw + PI + (frand() - 0.5f), 0);
        play3d(S_FAUX_BRUIT, v3(P.pos.x + f.x * 9, 1, P.pos.z + f.z * 9), 1.0f, 0.8f + 0.3f * frand(), 20);
        sursauter(BPM_SURSAUT * 0.3f);
    }
}

/* sons continus du monde (chaque image) : souffle du Rodeur ; parametres de la musique */
static void peur_sons(void) {
    if (!audio_ok) return;
    int enJeu = appState == APP_GAME && !cartonActif;
    for (int i = 0; i < nmon; i++) {
        Monstre *m = &mon[i];
        if (!enJeu || m->type != MON_RODEUR) { if (m->souffle) { loop_stop(m->souffle); m->souffle = 0; } continue; }
        if (!m->souffle) m->souffle = play_loop(S_RODEUR_SOUFFLE, 0, 1.0f);
        loop3d(m->souffle, m->pos, m->etat == E_CHASSE ? 1.1f : 0.65f, 9);
    }
    ambTension = enJeu ? tension : 0.15f;
    ambActif = appState == APP_GAME;
    ambMort = screamT >= 0 || mortT >= 0;
}
