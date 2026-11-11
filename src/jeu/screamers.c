/* BOURDONNEMENT - la peur : screamer de mort, apparitions dans le faisceau, faux bruits, coeur, drone,
   lampe qui gresille quand il est pres.
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   Regle d'or : la peur vient de l'attente. Les screamers sont rares et jamais deux fois de suite. */

static float screamT = -1;                                /* >= 0 : screamer de mort en cours (secondes) */
static float mortT = -1;                                  /* >= 0 : ecran de mort (secondes) */
static float apparitionT = -1;                            /* >= 0 : apparition en cours */
static V3 apparitionPos;
static float apparitionProchaine, fauxBruitProchain, coeurT;
static float lampeCoupure;                                /* > 0 : la lampe est coupee (apparition) */
static int droneVoix;
static float tension;                                     /* 0..1 : proximite et poursuite (son, effets d'ecran) */

static void peur_reset(void) {
    screamT = -1; mortT = -1; apparitionT = -1; lampeCoupure = 0; coeurT = 0; tension = 0;
    apparitionProchaine = APPARITION_MIN * 0.6f + frand() * (APPARITION_MAX - APPARITION_MIN) * 0.5f;
    fauxBruitProchain = FAUX_BRUIT_MIN + frand() * (FAUX_BRUIT_MAX - FAUX_BRUIT_MIN);
}

/* intensite de la lampe a cet instant (0..1) : batterie faible, monstre proche, apparition */
static float lampe_intensite(float t) {
    if (!P.lampe || lampeCoupure > 0 || apparitionT >= 0.28f) return 0;   /* apparition : vue, puis la lampe lache */
    float k = LAMPE_PUISSANCE;
    unsigned h = hash32((unsigned)(t * 30.0f));
    if (P.batterie < LAMPE_FAIBLE * LAMPE_BATTERIE && (h & 15) < 3) k *= 0.15f;
    float d = dist_xz(R.pos, P.pos);
    if (R.actif && d < LAMPE_MONSTRE_D) {                 /* il est pres : le faisceau sursaute */
        float p = (1 - d / LAMPE_MONSTRE_D) * 0.6f;
        if ((h >> 4) % 100 < (unsigned)(p * 100)) k *= 0.1f + 0.3f * ((h >> 12) & 1);
    }
    return k;
}

static void mort_commencer(void) {
    P.dead = 1; screamT = 0;
    play(S_HURLEMENT, 1.3f, 1.0f);
    play(S_HURLEMENT, 0.8f, 0.71f);                       /* doublee a la quinte grave : plus epais */
}

static void peur_tick(float dt) {
    if (attrape && !P.dead) mort_commencer();
    if (screamT >= 0) {
        screamT += dt;
        if (screamT > SCREAMER_MORT_T) { screamT = -1; mortT = 0; }
        return;
    }
    if (mortT >= 0) {
        mortT += dt;
        if (mortT > 4.5f || (mortT > 1.2f && (actP[A_UTILISER] || actP[A_LAMPE]))) niveau_charger(niveauNum);
        return;
    }
    float d = dist_xz(R.pos, P.pos);
    float proche = R.actif ? clampf(1 - d / 18.0f, 0, 1) : 0;
    float voulu = fmaxf(proche * 0.8f, R.etat == R_CHASSE ? 1.0f : 0.0f);
    tension += (voulu - tension) * fminf(1, dt * (voulu > tension ? 3.0f : 0.5f));

    if (d < COEUR_DIST && (coeurT -= dt) <= 0) {          /* coeur : plus vite et plus fort quand il approche */
        float c = 1 - d / COEUR_DIST;
        play(S_COEUR, 0.25f + 0.6f * c, 1.0f);
        coeurT = 1.05f - 0.6f * c - (R.etat == R_CHASSE ? 0.15f : 0);
    }
    if (lampeCoupure > 0) lampeCoupure -= dt;

    if (apparitionT >= 0) {                               /* apparition : il est la, au bout du faisceau... */
        apparitionT += dt;
        if (apparitionT > APPARITION_T) { apparitionT = -1; lampeCoupure = 0.45f; }   /* ... puis plus rien */
    } else if ((apparitionProchaine -= dt) <= 0) {
        apparitionProchaine = 15;                         /* conditions pas reunies : on reessaie bientot */
        V3 f = view_dir(P.yaw, 0);
        float libre = ray_grille(P.pos.x, P.pos.z, f.x, f.z, 12);
        if (R.etat != R_CHASSE && d > 10 && P.lampe && libre > 5.5f && fabsf(P.pitch) < 0.5f) {
            float at = fminf(libre - 1.0f, 5.0f + frand() * 3.0f);
            apparitionPos = v3(P.pos.x + f.x * at, 0, P.pos.z + f.z * at);
            apparitionT = 0;
            play(S_STINGER, 1.0f, 0.95f + 0.1f * frand());
            apparitionProchaine = APPARITION_MIN + frand() * (APPARITION_MAX - APPARITION_MIN);
            if (frand() < 0.5f) { rodeur_etat(R_ENQUETE, 0); rodeur_viser(P.pos); }   /* et le vrai arrive */
        }
    }
    if ((fauxBruitProchain -= dt) <= 0) {                 /* un choc lourd derriere toi, quelque part */
        fauxBruitProchain = FAUX_BRUIT_MIN + frand() * (FAUX_BRUIT_MAX - FAUX_BRUIT_MIN);
        V3 f = view_dir(P.yaw + PI + (frand() - 0.5f), 0);
        play3d(S_FAUX_BRUIT, v3(P.pos.x + f.x * 9, 1, P.pos.z + f.z * 9), 1.0f, 0.8f + 0.3f * frand(), 20);
    }
}

/* sons continus du monde (chaque image) : souffle du Rodeur, drone de tension */
static void peur_sons(void) {
    if (!audio_ok) return;
    if (appState != APP_GAME) {
        if (R.souffle) { loop_stop(R.souffle); R.souffle = 0; }
        if (droneVoix) { loop_stop(droneVoix); droneVoix = 0; }
        return;
    }
    if (!R.souffle) R.souffle = play_loop(S_RODEUR_SOUFFLE, 0, 1.0f);
    if (!droneVoix) droneVoix = play_loop(S_DRONE, 0, 1.0f);
    loop3d(R.souffle, R.pos, R.etat == R_CHASSE ? 1.1f : 0.65f, 9);
    loop_set(droneVoix, 0.08f + 0.5f * tension, 1.0f);
}
