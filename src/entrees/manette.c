/* BOURDONNEMENT - manette (PS4 d abord, Xbox et generiques aussi) : lecture, boutons -> touches K_PAD_*, visee au stick
   droit, curseur des menus au stick gauche.
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).

   Windows : API joystick de winmm (deja liee, aucune dependance). Une DualShock 4 branchee en USB ou en Bluetooth
   y apparait comme « Wireless Controller » : X/Y = stick gauche, Z/R = stick droit, U/V = L2/R2, 14 boutons, POV =
   croix directionnelle. Si Steam ou DS4Windows la font passer pour une manette Xbox, la disposition Xbox est lue.
   macOS : framework GameController (mac_pad_read). */

/* etat brut commun : axes -1..1 (lx vers la droite, ly vers le haut, idem rx, ry), gachettes 0..1,
   boutons : bit i = touche K_PAD_FIRST + i (croix directionnelle comprise, stick gauche non) */
typedef struct { float lx, ly, rx, ry, l2, r2; unsigned btn; } PadRaw;
#define PADB(k) (1u << ((k) - K_PAD_FIRST))

static unsigned padPrev;                                  /* boutons (et stick gauche) a la lecture precedente */

/* zone morte puis courbe au carre : precis au centre, rapide a fond */
static float pad_curve(float v) {
    float a = fabsf(v);
    if (a <= PAD_DEADZONE) return 0;
    a = fminf(1, (a - PAD_DEADZONE) / (1 - PAD_DEADZONE));
    return v < 0 ? -a * a : a * a;
}

#ifdef _WIN32
static int padJoy = -1, padSony;
static float padScanT;
static JOYCAPSA padCaps;
static float pad_norm(DWORD v, UINT mn, UINT mx) { return mx > mn ? clampf(2.0f * (float)(v - mn) / (float)(mx - mn) - 1, -1, 1) : 0; }
static int pad_read(PadRaw *r, float dt) {
    JOYINFOEX ji;
    if (padJoy < 0) {                                     /* recherche (pas a chaque image : couteux sans manette) */
        padScanT -= dt;
        if (padScanT > 0) return 0;
        padScanT = PAD_SCAN_T;
        UINT n = joyGetNumDevs();
        for (UINT i = 0; i < n && i < 16; i++) {
            memset(&ji, 0, sizeof ji); ji.dwSize = sizeof ji; ji.dwFlags = JOY_RETURNALL;
            if (joyGetPosEx(i, &ji) != JOYERR_NOERROR) continue;
            if (joyGetDevCapsA(i, &padCaps, sizeof padCaps) != JOYERR_NOERROR) continue;
            padJoy = (int)i;
            padSony = padCaps.wMid == 0x054C;            /* Sony : DualShock 4 (et DualSense) */
            snprintf(padName, sizeof padName, "%s", padSony ? "manette PlayStation" : padCaps.szPname);
            break;
        }
        if (padJoy < 0) return 0;
    }
    memset(&ji, 0, sizeof ji); ji.dwSize = sizeof ji; ji.dwFlags = JOY_RETURNALL;
    if (joyGetPosEx((UINT)padJoy, &ji) != JOYERR_NOERROR) { padJoy = -1; padScanT = 0; return 0; }   /* debranchee */
    const JOYCAPSA *c = &padCaps;
    memset(r, 0, sizeof *r);
    r->lx = pad_norm(ji.dwXpos, c->wXmin, c->wXmax);
    r->ly = -pad_norm(ji.dwYpos, c->wYmin, c->wYmax);
    if (padSony) {                                        /* DualShock 4 : Z/R stick droit, U/V gachettes */
        static const int map[14] = { K_PAD_SQUARE, K_PAD_CROSS, K_PAD_CIRCLE, K_PAD_TRIANGLE, K_PAD_L1, K_PAD_R1, K_PAD_L2,
                                     K_PAD_R2, K_PAD_SHARE, K_PAD_OPTIONS, K_PAD_L3, K_PAD_R3, K_PAD_PS, K_PAD_TOUCH };
        r->rx = pad_norm(ji.dwZpos, c->wZmin, c->wZmax);
        r->ry = -pad_norm(ji.dwRpos, c->wRmin, c->wRmax);
        r->l2 = (pad_norm(ji.dwUpos, c->wUmin, c->wUmax) + 1) * 0.5f;
        r->r2 = (pad_norm(ji.dwVpos, c->wVmin, c->wVmax) + 1) * 0.5f;
        for (int b = 0; b < 14; b++) if (ji.dwButtons & (1u << b)) r->btn |= PADB(map[b]);
    } else {                                              /* Xbox par winmm : U/R stick droit, Z = deux gachettes */
        static const int map[10] = { K_PAD_CROSS, K_PAD_CIRCLE, K_PAD_SQUARE, K_PAD_TRIANGLE, K_PAD_L1, K_PAD_R1,
                                     K_PAD_SHARE, K_PAD_OPTIONS, K_PAD_L3, K_PAD_R3 };
        float z = pad_norm(ji.dwZpos, c->wZmin, c->wZmax);
        r->rx = pad_norm(ji.dwUpos, c->wUmin, c->wUmax);
        r->ry = -pad_norm(ji.dwRpos, c->wRmin, c->wRmax);
        r->l2 = fmaxf(0, z); r->r2 = fmaxf(0, -z);
        for (int b = 0; b < 10; b++) if (ji.dwButtons & (1u << b)) r->btn |= PADB(map[b]);
    }
    if (ji.dwPOV != JOY_POVCENTERED) {                    /* croix directionnelle (centiemes de degre, 0 = haut) */
        DWORD a = ji.dwPOV;
        if (a >= 31500 || a <= 4500) r->btn |= PADB(K_PAD_UP);
        if (a >= 4500 && a <= 13500) r->btn |= PADB(K_PAD_RIGHT);
        if (a >= 13500 && a <= 22500) r->btn |= PADB(K_PAD_DOWN);
        if (a >= 22500 && a <= 31500) r->btn |= PADB(K_PAD_LEFT);
    }
    return 1;
}
static int pad_focus(void) { return GetForegroundWindow() == hwnd; }
#else
static int pad_read(PadRaw *r, float dt) {
    float ax[6];
    unsigned b = 0;
    (void)dt;
    memset(r, 0, sizeof *r);
    if (!mac_pad_read(ax, &b, padName, sizeof padName)) return 0;
    r->lx = ax[0]; r->ly = ax[1]; r->rx = ax[2]; r->ry = ax[3]; r->l2 = ax[4]; r->r2 = ax[5];
    r->btn = b;                                           /* mac_platform.m suit le meme ordre de bits */
    return 1;
}
static int pad_focus(void) { return mac_has_focus(); }
#endif

/* boutons, gachettes et stick gauche -> masque de touches K_PAD_* (stick gauche compris) */
static unsigned pad_mask(const PadRaw *r) {
    unsigned b = r->btn;
    if (r->l2 > PAD_TRIGGER_PRESS) b |= PADB(K_PAD_L2);
    if (r->r2 > PAD_TRIGGER_PRESS) b |= PADB(K_PAD_R2);
    if (r->ly > PAD_STICK_PRESS) b |= PADB(K_PAD_LS_UP);
    if (r->ly < -PAD_STICK_PRESS) b |= PADB(K_PAD_LS_DOWN);
    if (r->lx < -PAD_STICK_PRESS) b |= PADB(K_PAD_LS_LEFT);
    if (r->lx > PAD_STICK_PRESS) b |= PADB(K_PAD_LS_RIGHT);
    return b;
}

static void pad_release(void) {
    for (int k = K_PAD_FIRST; k < K_COUNT; k++) keys[k] = 0;
}

static void pad_assign(int code) {                        /* PARAMETRES > MANETTE : un bouton ne sert qu'a une action */
    if (waitPad < 0) return;
    if (code > 0)
        for (int a = 0; a < A_COUNT; a++)
            if (padBinds[a] == code) padBinds[a] = 0;
    padBinds[waitPad] = code;
    waitPad = -1;
}

/* une lecture : en jeu (souris captee) les touches K_PAD_* et la visee, sinon le curseur des menus.
   Separe de la lecture pour les tests (--test n'a pas de manette). */
static void pad_apply(const PadRaw *r, float dt) {
    unsigned b = pad_mask(r), pressed = b & ~padPrev, released = padPrev & ~b;
    padPrev = b;
    if (waitPad >= 0) {                                   /* reaffectation : le premier bouton appuye */
        pad_release();
        for (int k = K_PAD_FIRST; k < K_COUNT; k++)
            if (pressed & PADB(k)) { pad_assign(k); break; }
        return;
    }
    if (pressed & PADB(K_PAD_OPTIONS)) { in_key_down(SC_ESC, 0); in_key_up(SC_ESC); }   /* OPTIONS = ECHAP */
    if (captured) {
        for (int k = K_PAD_FIRST; k < K_COUNT; k++) {
            keys[k] = (b & PADB(k)) != 0;
            if (pressed & PADB(k)) latch[k] = 1;
        }
        if (appState == APP_GAME && !P.dead) {            /* visee au stick droit */
            P.yaw += pad_curve(r->rx) * padSens * dt;
            P.pitch = clampf(P.pitch + pad_curve(r->ry) * padSens * PAD_PITCH_K * dt * (padInvertY ? -1.0f : 1.0f), -1.55f, 1.55f);
        }
        return;
    }
    pad_release();                                        /* menus : curseur au stick gauche, CROIX = clic */
    float cx = pad_curve(r->lx), cy = pad_curve(r->ly);
    if (cx != 0 || cy != 0) {
        static float fx, fy;                              /* fractions de pixel */
        fx += cx * PAD_CURSOR_SPEED * dt; fy -= cy * PAD_CURSOR_SPEED * dt;
        int ix = (int)fx, iy = (int)fy;
        fx -= ix; fy -= iy;
        uiMX = uiMX + ix < 0 ? 0 : uiMX + ix >= winW ? winW - 1 : uiMX + ix;
        uiMY = uiMY + iy < 0 ? 0 : uiMY + iy >= winH ? winH - 1 : uiMY + iy;
        padCursor = 1;
    }
    if (pressed & PADB(K_PAD_CROSS)) { uiClick = 1; uiDown = 1; padCursor = 1; }
    if (released & PADB(K_PAD_CROSS)) { uiDown = 0; uiDrag = 0; }
    if (pressed & PADB(K_PAD_CIRCLE)) { in_key_down(SC_ESC, 0); in_key_up(SC_ESC); }   /* ROND = retour */
}

/* a chaque image, depuis la boucle principale (pas en mode bot, capture ou test) */
static void pad_frame(float dt) {
    PadRaw r;
    padOn = pad_read(&r, dt);
    if (!padOn || !pad_focus()) {                         /* rien, ou fenetre en arriere-plan : tout est relache */
        if (padPrev) pad_release();
        padPrev = 0;
        return;
    }
    pad_apply(&r, dt);
}
