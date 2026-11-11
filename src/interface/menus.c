/* BOURDONNEMENT - menus en mode immediat (comme VELOCITY) : ecran titre et pause
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   Un clic ne declenche rien tout de suite : il pose pendingAction, traite par do_action apres l'image. */

/* bouton centre en (cx, y) ; renvoie 1 au clic */
static int button(float cx, float y, const char *label, int action) {
    float w = 360, h = 52, x = cx - w / 2;
    int over = uiMX >= x && uiMX < x + w && uiMY >= y && uiMY < y + h;
    if (over) {
        rect(x, y, w, h, 0.85f, 0.78f, 0.38f, 0.10f);
        rect(x, y + h - 2, w, 2, 0.95f, 0.88f, 0.45f, 0.8f);
    }
    text_c(1, cx, y + 36, label, over ? 0.98f : 0.75f, over ? 0.93f : 0.70f, over ? 0.60f : 0.50f, 1);
    if (over && uiClick) { pendingAction = action; play(S_CLIC, 0.6f, 1.0f); return 1; }
    return 0;
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

static void draw_title(float t) {
    float cx = winW * 0.5f;
    rect_vgrad(0, 0, (float)winW, (float)winH, 0, 0, 0, 0.35f, 0.85f);
    float flick = (fmodf(t, 4.1f) > 3.95f) ? 0.35f : 1.0f;   /* le titre "grille" de temps en temps */
    text_c(2, cx, winH * 0.30f, "BOURDONNEMENT", 0.95f * flick, 0.88f * flick, 0.55f * flick, 1);
    text_c(0, cx, winH * 0.30f + 40, "Noe est entre aux Quatre Vents. Il n'est jamais ressorti.", 0.7f, 0.68f, 0.6f, 0.9f);
    float y = winH * 0.48f;
    button(cx, y, "ENTRER", ACT_JOUER); y += 64;
    button(cx, y, fullscreen ? "FENETRE" : "PLEIN ECRAN", ACT_PLEIN_ECRAN); y += 64;
    button(cx, y, "QUITTER", ACT_QUITTER);
    text(0, 24, winH - 16.0f, "casque conseille - accroupi (Ctrl), il n'entend que le bruit", 0.45f, 0.43f, 0.38f, 0.8f);
}

static void draw_pause(void) {
    float cx = winW * 0.5f;
    rect(0, 0, (float)winW, (float)winH, 0, 0, 0, 0.6f);
    text_c(2, cx, winH * 0.32f, "PAUSE", 0.9f, 0.85f, 0.6f, 1);
    float y = winH * 0.45f;
    button(cx, y, "REPRENDRE", ACT_REPRENDRE); y += 64;
    button(cx, y, "MENU PRINCIPAL", ACT_MENU); y += 64;
    button(cx, y, "QUITTER", ACT_QUITTER);
}
