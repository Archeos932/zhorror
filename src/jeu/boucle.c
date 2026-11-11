/* BOURDONNEMENT - game_tick : un pas de simulation (tick fixe 120 Hz, TICK)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static int bourdonVoice;                                  /* voix en boucle du bourdonnement des neons */
static int neonEtat = 1;                                  /* dernier etat du neon fou (pour son gresillement) */

static void reset_game(void) {
    player_reset();
    gameT = 0;
    neonEtat = 1;
}

static void game_tick(float dt) {
    input_poll();
    gameT += dt;
    player_tick(dt);
    int e = neon_allume(NEON_FOU, gameT);
    if (e != neonEtat) {
        if (!e) play(S_GRESILLE, 0.5f, 0.9f + 0.2f * frand());
        neonEtat = e;
    }
}

/* volume du bourdonnement selon l'ecran (appele a chaque image) */
static void ambiance_follow(void) {
    if (!audio_ok) return;
    if (!bourdonVoice) bourdonVoice = play_loop(S_BOURDON, BOURDON_VOL_MENU, 1.0f);
    float v = appState == APP_GAME ? BOURDON_VOL_JEU : BOURDON_VOL_MENU;
    if (appState == APP_GAME && !neonEtat) v *= 0.55f;    /* le neon fou coupe : le bourdon baisse avec lui */
    loop_set(bourdonVoice, v, 1.0f);
}
