/* BOURDONNEMENT - point d'entree (WinMain / bourdon_main) et boucle principale (repris de VELOCITY, allege)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static void do_action(void) {
    int a = pendingAction;
    pendingAction = ACT_NONE;
    switch (a) {
    case ACT_JOUER: reset_game(); appState = APP_GAME; set_capture(1); break;
    case ACT_REPRENDRE: set_capture(1); break;
    case ACT_MENU: set_capture(0); appState = APP_MENU; break;
    case ACT_QUITTER: PostQuitMessage(0); break;
    case ACT_PLEIN_ECRAN: toggle_fullscreen(); break;
    }
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
    (void)prev;
#else
static int bourdon_main(const char *cmd) {
#endif
    memcpy(binds, bindDefault, sizeof binds);
    memcpy(padBinds, padBindDefault, sizeof padBinds);
    if (cmd && strncmp(cmd, "--test ", 7) == 0) return run_tests(cmd + 7);   /* sans fenetre */

    /* --shot fichier.bmp : une image puis on quitte ; le nom choisit la scene ("menu", sinon en jeu) */
    const char *shotPath = NULL;
    static char shotBuf[512];
    if (cmd && strncmp(cmd, "--shot ", 7) == 0) {
        shotMode = 1;
        snprintf(shotBuf, sizeof shotBuf, "%s", cmd + 7);
        shotBuf[strcspn(shotBuf, " ")] = 0;
        shotPath = shotBuf;
    }

#ifndef _WIN32
    if (!mac_window_create(winW, winH, "BOURDONNEMENT", 1)) return 1;
    mac_window_size(&winW, &winH);
#else
    SetProcessDPIAware();
    WNDCLASSA wc = { 0 };
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "bourdonnement";
    RegisterClassA(&wc);
    RECT r = { 0, 0, winW, winH };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    int ww = r.right - r.left, wh = r.bottom - r.top;
    hwnd = CreateWindowA("bourdonnement", "BOURDONNEMENT", WS_OVERLAPPEDWINDOW,
                         (GetSystemMetrics(SM_CXSCREEN) - ww) / 2, (GetSystemMetrics(SM_CYSCREEN) - wh) / 2,
                         ww, wh, NULL, NULL, inst, NULL);
    hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = { sizeof pfd, 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
                                  PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 24, 8, 0,
                                  PFD_MAIN_PLANE, 0, 0, 0, 0 };
    SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
    HGLRC rc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, rc);
    typedef BOOL(WINAPI * SwapFn)(int);
    SwapFn swapInterval = (SwapFn)wglGetProcAddress("wglSwapIntervalEXT");
    if (swapInterval) swapInterval(1);
    ShowWindow(hwnd, shotMode ? SW_SHOWNOACTIVATE : show);
    UpdateWindow(hwnd);
    { RECT cr; GetClientRect(hwnd, &cr); winW = cr.right; winH = cr.bottom; }
    RAWINPUTDEVICE rid = { 0x01, 0x02, 0, hwnd };
    RegisterRawInputDevices(&rid, 1, sizeof rid);
#endif

    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glHint(GL_FOG_HINT, GL_NICEST);
    make_font(0, 18); make_font(1, 28); make_font(2, 56);
    for (int i = 0; i < 3; i++) make_font_atlas(i);
    make_sounds();
    reset_game();

    if (shotMode) {                                       /* capture : une image fixe, puis on quitte */
        if (strstr(shotPath, "menu")) { appState = APP_MENU; nowT = 20; }
        else { appState = APP_GAME; P.yaw = 0.35f; gameT = 1; }
        render(1, 0.016f);
        save_bmp(shotPath);
#ifndef _WIN32
        mac_shutdown();
#endif
        return 0;
    }
    audio_init();

    LARGE_INTEGER freq, start, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    last = start;
    float acc = 0, fpsAcc = 0; int fpsFrames = 0;
#ifdef _WIN32
    MSG msgw;
#endif
    for (;;) {
#ifdef _WIN32
        while (PeekMessageA(&msgw, NULL, 0, 0, PM_REMOVE)) {
            if (msgw.message == WM_QUIT) goto done;
            TranslateMessage(&msgw);
            DispatchMessageA(&msgw);
        }
#else
        if (!mac_pump()) goto done;
#endif
        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - last.QuadPart) / (float)freq.QuadPart;
        nowT = (double)(now.QuadPart - start.QuadPart) / (double)freq.QuadPart;
        last = now;
        if (dt > 0.1f) dt = 0.1f;
        fpsAcc += dt; fpsFrames++;
        if (fpsAcc > 0.5f) { fps = fpsFrames / fpsAcc; fpsAcc = 0; fpsFrames = 0; }

        pad_frame(dt);                                    /* manette : boutons, regard au stick droit, curseur des menus */
        if (appState == APP_GAME && captured) {
            if (!P.dead) {                                /* la souris tourne la vue a chaque image, pas a chaque tick */
                P.yaw += mdx * sens;
                P.pitch = clampf(P.pitch - mdy * sens * (invertY ? -1.0f : 1.0f), -1.50f, 1.50f);
            }
            mdx = mdy = 0;
            acc += dt;                                    /* tick fixe 120 Hz (solo : pause quand la souris est libre) */
            while (acc >= TICK) { game_tick(TICK); acc -= TICK; }
        } else {
            mdx = mdy = 0;
            memset(latch, 0, sizeof latch);
        }
        render(appState == APP_GAME && captured ? acc / TICK : 1.0f, dt);
        uiClick = 0;                                      /* apres le rendu : les boutons (mode immediat) ont vu le clic */
#ifdef _WIN32
        SwapBuffers(hdc);
#else
        mac_swap();
#endif
        ambiance_follow();
        audio_update();
        if (pendingAction) do_action();
    }
done:
    set_capture(0);
#ifdef _WIN32
    if (audio_ok) { waveOutReset(wout); waveOutClose(wout); }
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(rc);
#else
    if (audio_ok) mac_audio_close();
    mac_shutdown();
#endif
    return 0;
}

#ifndef _WIN32
int main(int argc, char **argv) {
    static char cmd[1024];                                /* la ligne de commande entiere, comme sous Windows */
    cmd[0] = 0;
    for (int i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "-psn_", 5)) continue;      /* argument ajoute par Finder sur les anciens macOS */
        size_t n = strlen(cmd);
        snprintf(cmd + n, sizeof cmd - n, "%s%s", n ? " " : "", argv[i]);
    }
    return bourdon_main(cmd);
}
#endif
