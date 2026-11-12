/* BOURDONNEMENT - game_tick : un pas de simulation (tick fixe 120 Hz, TICK)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static int bourdonVoice;                                  /* voix en boucle du bourdonnement des neons */
static unsigned char neonEtat[MAXNEON];                   /* dernier etat de chaque neon (gresillement au changement) */

/* mini-carte : les cases en vue (a moins de MINICARTE_VUE) sont decouvertes */
static void minicarte_decouvrir(void) {
    int px = (int)floorf(P.pos.x), pz = (int)floorf(P.pos.z), r = MINICARTE_VUE;
    for (int z = pz - r; z <= pz + r; z++)
        for (int x = px - r; x <= px + r; x++) {
            if (x < 0 || z < 0 || x >= carteW || z >= carteH || decouvert[z][x]) continue;
            if ((x - px) * (x - px) + (z - pz) * (z - pz) > r * r) continue;
            if (visible_xz(P.pos.x, P.pos.z, x + 0.5f, z + 0.5f)) decouvert[z][x] = 1;
        }
}

static void reset_game(void) {
    gameT = 0;
    niveau_charger_ex(1, 0);
    carton_ouvrir(texteIntro, "", 1);                     /* le texte d'ouverture, puis celui du niveau */
    for (int i = 0; i < nneons; i++) neonEtat[i] = (unsigned char)neon_allume(i, 0);
}

static void game_tick(float dt) {
    input_poll();
    if (carton_tick(dt)) { P.prevPos = P.pos; return; }   /* texte a l'ecran : le monde attend */
    gameT += dt;
    if (screamT < 0 && mortT < 0) player_tick(dt);
    else P.prevPos = P.pos;
    if (netMode == NET_CLIENT) monstres_client_tick(dt);  /* en ligne, c'est l'hote qui fait bouger les monstres */
    else monstres_tick(dt, gameT);
    objectif_tick(dt);
    pieces_tick(dt);
    {                                                     /* mini-carte : ce que l'on voit autour de soi */
        static float decT;
        if ((decT -= dt) <= 0) { decT = 0.15f; minicarte_decouvrir(); }
    }
    joueurs_tick(dt);
    reseau_hote_tick(dt);
    ecoutePos = P.pos; ecouteYaw = P.yaw;
    peur_tick(dt);
    for (int i = 0; i < nneons; i++) {                    /* les neons fous gresillent quand ils lachent */
        if (!neon_dynamique(i)) continue;
        int e = neon_allume(i, gameT);
        if (e != neonEtat[i]) {
            if (!e && neons[i].etat == NEON_FOU) play3d(S_GRESILLE, v3(neons[i].x, 2.7f, neons[i].z), 0.6f, 0.9f + 0.2f * frand(), 9);
            neonEtat[i] = (unsigned char)e;
        }
    }
}

/* sons continus (appele a chaque image) */
static void ambiance_follow(void) {
    if (!audio_ok) return;
    if (!bourdonVoice) bourdonVoice = play_loop(S_BOURDON, BOURDON_VOL_MENU, 1.0f);
    float v = BOURDON_VOL_MENU;
    if (appState == APP_GAME) {                           /* le bourdon suit la lumiere : quasi muet dans le noir */
        int n = 0;
        for (int i = 0; i < nneons; i++)
            if (neon_allume(i, gameT) && fabsf(neons[i].x - P.pos.x) < 6 && fabsf(neons[i].z - P.pos.z) < 6) n++;
        v = BOURDON_VOL_JEU * fminf(1, 0.15f + n * 0.3f) * (screamT >= 0 ? 0 : 1);
    }
    loop_set(bourdonVoice, v, 1.0f);
    peur_sons();
}
