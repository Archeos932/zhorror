/* BOURDONNEMENT - lecture des entrees (input_poll) et capture de la souris (repris de VELOCITY)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static void input_poll(void) {
    for (int a = 0; a < A_COUNT; a++) {
        act[a] = botHeld[a]; actP[a] = botPress[a];
        for (int k = 0; k < 2; k++) {
            int c = binds[a][k];
            if (c > 0) { act[a] |= keys[c]; actP[a] |= latch[c]; }
        }
        if (padBinds[a] > 0) { act[a] |= keys[padBinds[a]]; actP[a] |= latch[padBinds[a]]; }   /* manette */
    }
    memset(botPress, 0, sizeof botPress);
    memset(latch, 0, sizeof latch);
}

static float mdx, mdy, sens = 0.0022f;
static int captured, shotMode;

#ifdef _WIN32
static HWND hwnd;
static HDC hdc;
#endif
static int winW = 1600, winH = 900;

static void set_capture(int on) {
    if (on == captured) return;
    captured = on;
#ifndef _WIN32
    mac_set_capture(on);
    if (!on) memset(keys, 0, sizeof keys);
#else
    if (on) {
        RECT r; GetClientRect(hwnd, &r);
        POINT a = { r.left, r.top }, b = { r.right, r.bottom };
        ClientToScreen(hwnd, &a); ClientToScreen(hwnd, &b);
        RECT c = { a.x, a.y, b.x, b.y };
        ClipCursor(&c);
        while (ShowCursor(FALSE) >= 0) {}
        SetCapture(hwnd);
    } else {
        ClipCursor(NULL);
        while (ShowCursor(TRUE) < 0) {}
        ReleaseCapture();
        memset(keys, 0, sizeof keys);
    }
#endif
    mdx = mdy = 0;
}
