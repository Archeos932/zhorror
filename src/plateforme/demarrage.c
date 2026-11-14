/* BOURDONNEMENT - point d'entree (WinMain / bourdon_main) et boucle principale (repris de VELOCITY, allege)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* --multi creer|rejoindre : deux instances jouent seules (tests du reseau sur une machine) ; autoMulti : 1 creer, 2 rejoindre */
static float autoTemps = 30, autoT, autoLogT;
static void auto_tick(float dt) {
    if (!autoMulti) return;
    autoT += dt;
    if (appState == APP_MENU && autoT > 0.5f && salEtat == SAL_AUCUN) { en_ligne_ouvrir(); appState = APP_EN_LIGNE; }
    if (salEtat == SAL_LISTE) {
        if (autoMulti == 1) { salon_creer(); salEtat = SAL_CONNEXION; }
        else if (nsalons > 0) { salon_rejoindre(salons[0].code); salEtat = SAL_CONNEXION; }
    }
    if (salEtat == SAL_DANS && salHote) {                 /* l'hote lance des que quelqu'un est la */
        int n = 0; for (int k = 0; k < MAXP; k++) n += JP[k].nom[0] != 0;
        static float relance;
        if (n >= 2 && (relance -= dt) <= 0) { salon_lancer(); relance = 2; }
    }
    if (appState == APP_GAME) {
        if (cartonActif && fmodf(autoT, 0.4f) < dt) botPress[A_UTILISER] = 1;   /* lit les textes en vitesse */
        botHeld[A_AVANCER] = !cartonActif && fmodf(autoT, 6) < 4;   /* il marche, s'arrete, tourne */
        if (!cartonActif && fmodf(autoT, 6) > 5.5f) P.yaw += dt * 2.5f;
        if ((autoLogT -= dt) <= 0) {
            autoLogT = 2;
            char autres[160] = ""; int k0 = 0;
            for (int k = 0; k < MAXP; k++)
                if (k != monSlot && JP[k].actif)
                    k0 += snprintf(autres + k0, sizeof autres - (size_t)k0, " %s(%.1f,%.1f)", JP[k].nom, JP[k].st.x, JP[k].st.z);
            netlog("etat : niveau %d, moi (%.1f,%.1f) %s, autres :%s, monstres %d [%d a (%.1f,%.1f)], recus %d envoyes %d",
                   niveauNum, P.pos.x, P.pos.z, P.dead ? "mort" : "vivant", autres, nmon, nmon ? mon[0].type : -1,
                   nmon ? mon[0].pos.x : 0, nmon ? mon[0].pos.z : 0, netRecus, netEnvoyes);
        }
    }
    if (autoT > autoTemps) {
        netlog("fin : %s, place %d, niveau %d, recus %d, envoyes %d", netMode == NET_HOTE ? "hote" : netMode == NET_CLIENT ? "client" : "hors ligne",
               monSlot, niveauNum, netRecus, netEnvoyes);
        PostQuitMessage(0);
    }
}

static void do_action(void) {
    int a = pendingAction;
    pendingAction = ACT_NONE;
    switch (a) {
    case ACT_JOUER: reset_game(); appState = APP_GAME; set_capture(1); break;
    case ACT_REPRENDRE: set_capture(1); break;
    case ACT_MENU: set_capture(0); appState = APP_MENU; settingsOpen = 0; if (netMode != NET_OFF) en_ligne_fermer(); break;
    case ACT_QUITTER: en_ligne_fermer(); PostQuitMessage(0); break;
    case ACT_PLEIN_ECRAN: toggle_fullscreen(); cfg_save(); break;
    case ACT_PARAM: settingsOpen = 1; break;
    case ACT_FERMER_PARAM: settingsOpen = 0; cfg_save(); break;
    case ACT_MAJ: upd_click(); break;
    case ACT_EN_LIGNE: cfg_save(); en_ligne_ouvrir(); appState = APP_EN_LIGNE; uiFocus = 0; break;
    case ACT_CREER: { char n[16]; nom_propre(n, sizeof n); ws_envoyer("nom %s", n); salon_creer(); cfg_save(); break; }
    case ACT_REJOINDRE:
        if (pendingArg >= 0 && pendingArg < nsalons) { char n[16]; nom_propre(n, sizeof n); ws_envoyer("nom %s", n); salon_rejoindre(salons[pendingArg].code); }
        break;
    case ACT_REJOINDRE_CODE:
        if (strlen(codeSaisi) == 4) { char n[16]; nom_propre(n, sizeof n); ws_envoyer("nom %s", n); salon_rejoindre(codeSaisi); }
        else set_status("le code d'un salon a 4 lettres");
        break;
    case ACT_LANCER: salon_lancer(); break;
    case ACT_QUITTER_SALON: ws_envoyer("quitter"); salEtat = SAL_LISTE; salCode[0] = 0; memset(JP, 0, sizeof JP); salListeT = 0; break;
    case ACT_RETOUR: en_ligne_fermer(); appState = APP_MENU; uiFocus = 0; break;
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
        carteTest = !strstr(shotPath, "hall") && !strstr(shotPath, "menu");   /* "hall" : le vrai niveau 1 */
    }
    {                                                     /* pseudo par defaut : le nom de la session */
#ifdef _WIN32
        DWORD un = sizeof monNom;
        if (!GetUserNameA(monNom, &un) || !monNom[0]) snprintf(monNom, sizeof monNom, "joueur");
#else
        struct passwd *pw = getpwuid(getuid());
        snprintf(monNom, sizeof monNom, "%s", pw && pw->pw_name[0] ? pw->pw_name : "joueur");
#endif
        monNom[15] = 0;
    }
    if (cmd && strstr(cmd, "--updated")) updAfter = 1;   /* relance apres une mise a jour */
    if (cmd && strstr(cmd, "--multi")) {                  /* --multi creer|rejoindre [--serveur h:p] [--nom N] [--temps S] [--log f] */
        static char args[1024];
        snprintf(args, sizeof args, "%s", cmd);
        for (char *t = strtok(args, " "); t; t = strtok(NULL, " ")) {
            if (!strcmp(t, "--multi") && (t = strtok(NULL, " "))) autoMulti = !strcmp(t, "creer") ? 1 : 2;
            else if (!strcmp(t, "--serveur") && (t = strtok(NULL, " "))) snprintf(serveur, sizeof serveur, "%s", t);
            else if (!strcmp(t, "--nom") && (t = strtok(NULL, " "))) snprintf(monNom, sizeof monNom, "%s", t);
            else if (!strcmp(t, "--temps") && (t = strtok(NULL, " "))) autoTemps = (float)atof(t);
            else if (!strcmp(t, "--log") && (t = strtok(NULL, " "))) netLog = fopen(t, "w");
        }
    }

#ifndef _WIN32
    if (!mac_window_create(winW, winH, "BOURDONNEMENT", 1)) return 1;
    if (autoMulti) mac_window_place(autoMulti == 1 ? 0 : 820, 40, 800, 450);   /* deux fenetres cote a cote */
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
    make_textures();
    glReady = 1;
    make_sounds();
    reset_game();

    if (shotMode) {                                       /* capture : une image fixe, puis on quitte */
        appState = APP_GAME; gameT = 30; msgT = 0; cartonActif = 0;
        for (int i = 0; i < nmon; i++) mon[i].pos = mon[i].prevPos = v3(20.5f, 0, 31.5f);   /* loin, sauf scenes de monstres */
        bpm = 96; coeurPouls = 0.6f;
        if (strstr(shotPath, "menu")) { appState = APP_MENU; nowT = 20; }
        else if (strstr(shotPath, "param")) { appState = APP_MENU; nowT = 20; settingsOpen = 1; micOn = 1; micOuvert = 1; micNiveau = 0.06f; }
        else if (strstr(shotPath, "carton")) { carton_niveau(2); cartonCars = 200; nowT = 3; }
        else if (strstr(shotPath, "noir")) { P.pos = P.prevPos = v3(40.5f, 0, 7.5f); P.yaw = P.lampeYaw = 0; }   /* zone sans neon, flaque */
        else if (strstr(shotPath, "piege")) { P.pos = P.prevPos = v3(31.2f, 0, 27.5f); P.yaw = P.lampeYaw = PI / 2; P.pitch = P.lampePitch = -0.45f; }
        else if (strstr(shotPath, "flaque")) { P.pos = P.prevPos = v3(7.0f, 0, 16.5f); P.yaw = P.lampeYaw = PI / 2; P.pitch = P.lampePitch = -0.3f; }
        else if (strstr(shotPath, "rodeur") || strstr(shotPath, "locataire") || strstr(shotPath, "rampante") || strstr(shotPath, "screamer")) {
            int type = strstr(shotPath, "locataire") ? MON_LOCATAIRE : strstr(shotPath, "rampante") ? MON_RAMPANTE : MON_RODEUR;
            P.pos = P.prevPos = v3(10.5f, 0, 27.5f); P.yaw = P.lampeYaw = PI / 2;
            monstres_reset(); mon_ajouter(type, 1);
            mon[0].pos = mon[0].prevPos = v3(type == MON_RAMPANTE ? 13.5f : 14.2f, 0, 27.6f); mon[0].yaw = -PI / 2; mon[0].anim = 0.7f;
            mon[0].etat = type == MON_LOCATAIRE ? E_FIGE : E_ERRE;
            if (strstr(shotPath, "screamer")) { tueur = 0; screamType = type; screamT = 0.2f; P.dead = 1; }
        } else if (strstr(shotPath, "ascenseur")) {
            P.pos = P.prevPos = v3(43.2f, 0, 1.5f); P.yaw = P.lampeYaw = PI / 2;
            asc.alimente = 1; asc.ouverture = 0.7f;
        } else if (strstr(shotPath, "piliers")) { P.pos = P.prevPos = v3(2.5f, 0, 29.5f); P.yaw = P.lampeYaw = PI / 2 - 0.25f; }
        else if (strstr(shotPath, "niveau3")) { niveau_charger_ex(3, 0); for (int i = 0; i < nmon; i++) mon[i].pos = v3(-50, 0, -50); }
        else if (strstr(shotPath, "hall")) {             /* le vrai niveau 1, au depart, puis l'open space */
            if (strstr(shotPath, "hall2")) { P.pos = P.prevPos = v3(9.5f, 0, 5.5f); P.yaw = P.lampeYaw = PI / 2 + 0.15f; }
            if (strstr(shotPath, "hall3")) { P.pos = P.prevPos = v3(2.5f, 0, 14.5f); P.yaw = P.lampeYaw = PI / 2; }
            if (strstr(shotPath, "hallpiece") && npieces > 0) {   /* a la porte de la chambre du niveau 1 */
                Piece *pc = &pieces[strstr(shotPath, "hallpiece2") ? 2 : 0];
                V3 c = v3(pc->x0 + pc->iw * 0.5f, 0, pc->z0 + pc->ih * 0.5f), pt = v3(pc->porteX + 0.5f, 0, pc->porteZ + 0.5f);
                V3 d = vnorm(vsub(c, pt));
                P.pos = P.prevPos = vadd(pt, vmul(d, 0.7f)); P.yaw = P.lampeYaw = atan2f(d.x, -d.z); P.pitch = P.lampePitch = -0.12f;
                monstres_reset();
                for (int k = 0; k < 6; k++) render(1, 0.016f);   /* les ecrans de la salle des cameras */
            }
        }
        else if (strstr(shotPath, "chambre") || strstr(shotPath, "sdb") || strstr(shotPath, "enfant") || strstr(shotPath, "cuisine") ||
                 strstr(shotPath, "reserve") || strstr(shotPath, "noe")) {   /* vitrine des pieces : a la porte, on regarde dedans */
            int z = strstr(shotPath, "chambre") ? ZONE_CHAMBRE : strstr(shotPath, "sdb") ? ZONE_SDB : strstr(shotPath, "enfant") ? ZONE_ENFANT :
                    strstr(shotPath, "cuisine") ? ZONE_CUISINE : strstr(shotPath, "reserve") ? ZONE_RESERVE : ZONE_NOE;
            charger_vitrine(); monstres_reset(); nfus = 0;
            int ip = piece_de_zone(z);
            if (ip >= 0) {
                Piece *pc = &pieces[ip];
                V3 c = v3(pc->x0 + pc->iw * 0.5f, 0, pc->z0 + pc->ih * 0.5f), pt = v3(pc->porteX + 0.5f, 0, pc->porteZ + 0.5f);
                V3 d = vnorm(vsub(c, pt));
                P.pos = P.prevPos = vadd(pt, vmul(d, 0.7f)); P.yaw = P.lampeYaw = atan2f(d.x, -d.z); P.pitch = P.lampePitch = -0.12f;
            }
            monde_reconstruire();
        }
        else if (strstr(shotPath, "cameras")) {           /* salle des cameras de la vitrine : les ecrans en direct */
            charger_vitrine(); monstres_reset(); nfus = 0; monde_reconstruire();
            int ip = piece_de_zone(ZONE_CAMERAS);
            if (ip >= 0) {
                Piece *pc = &pieces[ip];
                for (int i = 0; i < nmeubles; i++) if (meubles[i].type == MB_ECRANS) {
                    V3 av = view_dir(meubles[i].yaw, 0);
                    P.pos = P.prevPos = v3(meubles[i].x + av.x * 1.6f, 0, meubles[i].z + av.z * 1.6f);
                    P.yaw = P.lampeYaw = meubles[i].yaw + PI; P.pitch = P.lampePitch = -0.05f; P.lampe = 0;
                }
                (void)pc;
            }
            monstres_reset(); mon_ajouter(MON_RODEUR, 1);   /* il traine devant une camera */
            mon[0].pos = mon[0].prevPos = v3(cameras[1].pos.x + sinf(cameras[1].yaw) * 3.5f, 0, cameras[1].pos.z - cosf(cameras[1].yaw) * 3.5f);
            mon[0].yaw = cameras[1].yaw + PI;
                for (int k = 0; k < 6; k++) render(1, 0.016f);
        }
        else if (strstr(shotPath, "armoire") || strstr(shotPath, "souslit")) {   /* cachettes de la chambre (vitrine) */
            charger_vitrine(); monstres_reset(); nfus = 0; monde_reconstruire();
            for (int i = 0; i < nmeubles; i++) {
                const Meuble *m = &meubles[i];
                if (strstr(shotPath, "armoire") && m->type == MB_ARMOIRE && pieces[m->piece].zone == ZONE_CHAMBRE) {
                    P.cache = 2; P.armoire = i; P.pos = P.prevPos = v3(m->x, 0, m->z); P.yaw = P.lampeYaw = m->yaw + 0.25f; P.lampe = 0;
                    P.oeil = P.prevOeil = OEIL_DEBOUT;
                }
                if (strstr(shotPath, "souslit") && m->type == MB_LIT) {
                    P.cache = 1; P.pos = P.prevPos = v3(m->x, 0, m->z); P.yaw = P.lampeYaw = m->yaw; P.lampe = 0;
                    P.oeil = P.prevOeil = OEIL_SOUS_LIT; P.accroupi = 1;
                    monstres_reset(); mon_ajouter(MON_RODEUR, 1);   /* il passe devant le lit */
                    V3 av = view_dir(m->yaw, 0);
                    mon[0].pos = mon[0].prevPos = v3(m->x + av.x * 1.9f + 0.3f, 0, m->z + av.z * 1.9f); mon[0].yaw = m->yaw + PI / 2; mon[0].anim = 0.4f;
                }
            }
        }
        else if (strstr(shotPath, "spectateur")) {      /* mort en ligne : on suit Ines, derriere elle */
            netMode = NET_HOTE; monSlot = 0; P.dead = 1; mortT = 3; screamType = MON_RODEUR;
            JP[1].actif = 1; snprintf(JP[1].nom, sizeof JP[1].nom, "Ines");
            JP[1].st.x = 6.5f; JP[1].st.z = 27.8f; JP[1].st.yaw = JP[1].st.lampeYaw = PI / 2 + 0.25f; JP[1].st.oeil = 165;
            JP[1].st.flags = JF_LAMPE; JP[1].disp = v3(6.5f, 0, 27.8f);
        }
        else if (strstr(shotPath, "multi")) {           /* un coequipier, sa lampe, son pseudo */
            netMode = NET_HOTE; monSlot = 0;
            P.pos = P.prevPos = v3(2.5f, 0, 27.5f); P.yaw = P.lampeYaw = PI / 2;
            JP[1].actif = 1; snprintf(JP[1].nom, sizeof JP[1].nom, "Ines");
            JP[1].st.x = 6.5f; JP[1].st.z = 27.8f; JP[1].st.yaw = JP[1].st.lampeYaw = PI / 2 + 0.25f; JP[1].st.oeil = 165;
            JP[1].st.flags = JF_LAMPE; JP[1].disp = v3(6.5f, 0, 27.8f);
            P.lampe = 0;
        }
        else if (strstr(shotPath, "enligne")) { appState = APP_EN_LIGNE; nowT = 20; salEtat = SAL_DANS; salHote = 1; snprintf(salCode, sizeof salCode, "HJAK");
            snprintf(JP[0].nom, 16, "Mathis"); snprintf(JP[1].nom, 16, "Ines"); snprintf(JP[2].nom, 16, "Sami"); }
        else if (strstr(shotPath, "mort")) { screamType = MON_RAMPANTE; mortT = 2; P.dead = 1; }
        minicarte_decouvrir();
        render(1, 0.016f);
        save_bmp(shotPath);
#ifndef _WIN32
        mac_shutdown();
#endif
        return 0;
    }
    {                                                     /* bourdonnement.ini */
#ifdef _WIN32
        char *slash;
        GetModuleFileNameA(NULL, cfgPath, sizeof cfgPath);
        slash = strrchr(cfgPath, '\\');
        if (slash) strcpy(slash + 1, "bourdonnement.ini"); else cfgPath[0] = 0;
#elif defined(__linux__)
        char dir[MAX_PATH - 24];
        if (linux_config_dir(dir, sizeof dir, 1)) snprintf(cfgPath, sizeof cfgPath, "%s/bourdonnement.ini", dir);
        else cfgPath[0] = 0;
#else                                                     /* le .app peut etre en lecture seule : reglages dans ~/Library */
        const char *home = getenv("HOME");
        if (home) {
            snprintf(cfgPath, sizeof cfgPath, "%s/Library/Application Support/Bourdonnement", home);
            mkdir(cfgPath, 0755);
            snprintf(cfgPath, sizeof cfgPath, "%s/Library/Application Support/Bourdonnement/bourdonnement.ini", home);
        } else cfgPath[0] = 0;
#endif
        if (autoMulti) cfgPath[0] = 0;                    /* les tests ne touchent pas aux reglages du joueur */
        if (cfg_load()) toggle_fullscreen();
    }
    audio_init();
    micro_appliquer();
    upd_cleanup();                                        /* restes d'une mise a jour precedente */
    if (updAfter) {
        char ub[64];
        snprintf(ub, sizeof ub, "mise a jour installee : build %d", BOURDON_BUILD);
        set_status(ub);
    }
    if (updCheck) { updQuiet = 1; upd_start(upd_check_th, UPD_CHECKING); }   /* verification discrete */

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
        reseau_maj(dt);                                   /* salons et partie en ligne */
        auto_tick(dt);
        if (appState == APP_GAME && (captured || netMode != NET_OFF)) {   /* en ligne, pas de pause */
            if (!P.dead && captured) {                    /* la souris tourne la vue a chaque image, pas a chaque tick */
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
        render(appState == APP_GAME && (captured || netMode != NET_OFF) ? acc / TICK : 1.0f, dt);
        uiClick = 0;                                      /* apres le rendu : les boutons (mode immediat) ont vu le clic */
#ifdef _WIN32
        SwapBuffers(hdc);
#else
        mac_swap();
#endif
        ambiance_follow();
        pieces_sons(dt);
        audio_update();
        micro_maj(dt);
        if (pendingAction) do_action();
    }
done:
    set_capture(0);
    cfg_save();
    micOn = 0; micro_appliquer();
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
