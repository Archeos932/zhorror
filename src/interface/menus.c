/* BOURDONNEMENT - menus en mode immediat (comme VELOCITY) : ecran titre, pause, PARAMETRES, mise a jour
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   Un clic ne declenche rien tout de suite : il pose pendingAction, traite par do_action apres l'image.
   Les curseurs et interrupteurs de PARAMETRES changent le reglage tout de suite (sauvegarde a la fermeture). */

/* bouton centre en (cx, y) ; renvoie 1 au clic */
static int button(float cx, float y, const char *label, int action) {
    float w = 420, h = 52, x = cx - w / 2;
    int over = uiMX >= x && uiMX < x + w && uiMY >= y && uiMY < y + h;
    if (over) {
        rect(x, y, w, h, 0.85f, 0.78f, 0.38f, 0.10f);
        rect(x, y + h - 2, w, 2, 0.95f, 0.88f, 0.45f, 0.8f);
    }
    text_c(1, cx, y + 36, label, over ? 0.98f : 0.75f, over ? 0.93f : 0.70f, over ? 0.60f : 0.50f, 1);
    if (over && uiClick) { pendingAction = action; play(S_CLIC, 0.6f, 1.0f); return 1; }
    return 0;
}

/* curseur : libelle a gauche, barre a droite ; glisser pour regler. id : pour garder la prise pendant le glisser */
static int slideTenu = -1;
static void curseur(int id, float cx, float y, const char *label, float *v, float lo, float hi, const char *valeur) {
    float x = cx - 360, bx = cx + 20, bw = 300;
    text(0, x, y + 22, label, 0.78f, 0.74f, 0.6f, 1);
    int over = uiMX >= bx - 10 && uiMX < bx + bw + 10 && uiMY >= y && uiMY < y + 32;
    if (over && uiClick) slideTenu = id;
    if (!uiDown && slideTenu == id) slideTenu = -1;
    if (slideTenu == id) *v = lo + clampf((uiMX - bx) / bw, 0, 1) * (hi - lo);
    float k = (*v - lo) / (hi - lo);
    rect(bx, y + 14, bw, 4, 0.3f, 0.28f, 0.22f, 0.8f);
    rect(bx, y + 14, bw * k, 4, 0.9f, 0.82f, 0.45f, 0.9f);
    rect(bx + bw * k - 5, y + 6, 10, 20, over || slideTenu == id ? 1.0f : 0.8f, 0.9f, 0.6f, 1);
    text(0, bx + bw + 20, y + 22, valeur, 0.65f, 0.62f, 0.5f, 1);
}

/* interrupteur OUI / NON */
static void interrupteur(float cx, float y, const char *label, int *v) {
    float x = cx - 360, bx = cx + 20;
    text(0, x, y + 22, label, 0.78f, 0.74f, 0.6f, 1);
    int over = uiMX >= bx && uiMX < bx + 140 && uiMY >= y && uiMY < y + 32;
    if (over) rect(bx, y, 140, 32, 0.85f, 0.78f, 0.38f, 0.10f);
    text(0, bx + 10, y + 22, *v ? "[ OUI ]" : "[ NON ]", *v ? 0.9f : 0.6f, *v ? 0.85f : 0.55f, *v ? 0.5f : 0.45f, 1);
    if (over && uiClick) { *v = !*v; play(S_CLIC, 0.6f, 1.0f); }
}

/* incrustation de camescope : REC qui clignote et horodatage fige de la nuit de la disparition */
static void draw_vhs_overlay(float t) {
    if (fmodf(t, 1.2f) < 0.8f) {
        rect(42, 38, 16, 16, 0.9f, 0.1f, 0.08f, 0.95f);
        text(1, 68, 56, "REC", 0.95f, 0.95f, 0.9f, 0.9f);
    }
    char ts[48];
    int s = (int)t, m = 47 + s / 60;
    snprintf(ts, sizeof ts, "OCT 14 2003  01:%02d:%02d AM", m % 60, s % 60);
    text(0, winW - 40 - fontW[0] * (float)strlen(ts), winH - 40.0f, ts, 0.95f, 0.95f, 0.9f, 0.85f);
}

static void draw_statut(void) {
    if (statut[0] && nowT - statutT < 8) text_c(0, winW * 0.5f, winH - 50.0f, statut, 0.85f, 0.8f, 0.6f, fminf(1, (float)(8 - (nowT - statutT))));
}

static void draw_settings(void) {
    float cx = winW * 0.5f, y = winH * 0.16f;
    char b[64];
    rect(0, 0, (float)winW, (float)winH, 0, 0, 0, 0.82f);
    text_c(2, cx, y, "PARAMETRES", 0.9f, 0.85f, 0.6f, 1);
    y += 50;
    snprintf(b, sizeof b, "%d %%", (int)(masterVol * 100 + 0.5f));
    curseur(0, cx, y, "VOLUME", &masterVol, 0, 1.5f, b); y += 46;
    snprintf(b, sizeof b, "%d %%", (int)(ambVol * 100 + 0.5f));
    curseur(1, cx, y, "MUSIQUE", &ambVol, 0, 1.5f, b); y += 46;
    snprintf(b, sizeof b, "%.1f", sens * 1000);
    curseur(2, cx, y, "SENSIBILITE SOURIS", &sens, 0.0005f, 0.006f, b); y += 46;
    interrupteur(cx, y, "INVERSER L'AXE VERTICAL", &invertY); y += 46;
    {
        int avant = micOn;
        interrupteur(cx, y, "MICRO (il entend ta voix)", &micOn);
        if (micOn != avant) micro_appliquer();
        y += 46;
    }
    snprintf(b, sizeof b, "x %.1f", micGain);
    curseur(3, cx, y, "SENSIBILITE DU MICRO", &micGain, 0.5f, 12, b); y += 46;
    {                                                     /* le niveau du micro, en direct, avec le seuil du chuchotement */
        float bx = cx + 20, bw = 300, v = clampf(micNiveau * micGain / (MICRO_SEUIL + MICRO_PLAGE * MICRO_MAX), 0, 1);
        float seuil = MICRO_SEUIL / (MICRO_SEUIL + MICRO_PLAGE * MICRO_MAX);
        text(0, cx - 360, y + 18, micOuvert ? "TA VOIX" : "TA VOIX (micro coupe)", 0.6f, 0.58f, 0.5f, 1);
        rect(bx, y + 8, bw, 10, 0.15f, 0.14f, 0.12f, 0.9f);
        rect(bx, y + 8, bw * v, 10, v > seuil ? 0.9f : 0.4f, v > seuil ? 0.2f : 0.7f, v > seuil ? 0.1f : 0.4f, 0.95f);
        rect(bx + bw * seuil, y + 4, 2, 18, 1, 1, 1, 0.8f);
        text(0, bx + bw + 20, y + 18, micro_bruit() > 0 ? "IL T'ENTEND" : "chuchote", 0.65f, 0.62f, 0.5f, 1);
        y += 40;
    }
    interrupteur(cx, y, "FLASHS ET IMAGES SUBLIMINALES", &flashsOn); y += 46;
    interrupteur(cx, y, "VERIFIER LES MISES A JOUR", &updCheck); y += 56;
    button(cx, y, "RETOUR", ACT_FERMER_PARAM);
}

static void draw_title(float t) {
    float cx = winW * 0.5f;
    rect_vgrad(0, 0, (float)winW, (float)winH, 0, 0, 0, 0.35f, 0.85f);
    if (settingsOpen) { draw_settings(); draw_statut(); return; }
    float flick = (fmodf(t, 4.1f) > 3.95f) ? 0.35f : 1.0f;   /* le titre "grille" de temps en temps */
    text_c(2, cx, winH * 0.26f, "BOURDONNEMENT", 0.95f * flick, 0.88f * flick, 0.55f * flick, 1);
    text_c(0, cx, winH * 0.26f + 40, "Noe est entre aux Quatre Vents. Il n'est jamais ressorti.", 0.7f, 0.68f, 0.6f, 0.9f);
    float y = winH * 0.40f;
    button(cx, y, "ENTRER", ACT_JOUER); y += 60;
    button(cx, y, "PARAMETRES", ACT_PARAM); y += 60;
    button(cx, y, upd_label(), ACT_MAJ); y += 60;
    button(cx, y, fullscreen ? "FENETRE" : "PLEIN ECRAN", ACT_PLEIN_ECRAN); y += 60;
    button(cx, y, "QUITTER", ACT_QUITTER);
    upd_poll();
    draw_statut();
    text(0, 24, winH - 16.0f, "casque conseille - Ctrl accroupi, F lampe, E ramasser, A/E se pencher", 0.45f, 0.43f, 0.38f, 0.8f);
}

static void draw_pause(void) {
    float cx = winW * 0.5f;
    rect(0, 0, (float)winW, (float)winH, 0, 0, 0, 0.6f);
    if (settingsOpen) { draw_settings(); return; }
    text_c(2, cx, winH * 0.30f, "PAUSE", 0.9f, 0.85f, 0.6f, 1);
    float y = winH * 0.42f;
    button(cx, y, "REPRENDRE", ACT_REPRENDRE); y += 60;
    button(cx, y, "PARAMETRES", ACT_PARAM); y += 60;
    button(cx, y, "MENU PRINCIPAL", ACT_MENU); y += 60;
    button(cx, y, "QUITTER", ACT_QUITTER);
}
