/* BOURDONNEMENT - render() : l'image complete
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* assombrit les bords de l'ecran (vignette), plus fort avec la tension */
static void vignette(float force) {
    float w = (float)winW, h = (float)winH, b = w * 0.22f;
    rect_vgrad(0, 0, w, h * 0.3f, 0, 0, 0, force, 0);
    rect_vgrad(0, h * 0.7f, w, h * 0.3f, 0, 0, 0, 0, force);
    glBegin(GL_QUADS);                                    /* bords gauche et droit */
    glColor4f(0, 0, 0, force); glVertex2f(0, 0); glColor4f(0, 0, 0, 0); glVertex2f(b, 0); glVertex2f(b, h); glColor4f(0, 0, 0, force); glVertex2f(0, h);
    glColor4f(0, 0, 0, 0); glVertex2f(w - b, 0); glColor4f(0, 0, 0, force); glVertex2f(w, 0); glVertex2f(w, h); glColor4f(0, 0, 0, 0); glVertex2f(w - b, h);
    glEnd();
}

/* un fusible a portee, devant ? (invite "E") */
static int fusible_vise(void) {
    V3 f = view_dir(P.yaw, 0);
    for (int i = 0; i < nfus; i++) {
        if (fus[i].pris) continue;
        V3 d = vsub(fus[i].pos, P.pos); d.y = 0;
        float l = hlen(d);
        if (l < RAMASSER_DIST && (l < 0.8f || (d.x * f.x + d.z * f.z) / l > 0.3f)) return 1;
    }
    return 0;
}

static void draw_hud(float t) {
    char b[96];
    /* objectif, discret en haut a gauche */
    if (fusPris < nfus) snprintf(b, sizeof b, "FUSIBLES  %d / %d", fusPris, nfus);
    else snprintf(b, sizeof b, "L'ASCENSEUR");
    text(0, 28, 44, b, 0.75f, 0.72f, 0.6f, 0.55f);
    /* bruit : ce qu'il peut entendre */
    float br = clampf(P.bruit / 1.2f, 0, 1);
    text(0, 28, winH - 52.0f, "BRUIT", 0.6f, 0.58f, 0.5f, 0.5f);
    rect(28, winH - 42.0f, 110, 4, 0.2f, 0.2f, 0.2f, 0.4f);
    rect(28, winH - 42.0f, 110 * br, 4, 0.5f + 0.5f * br, 0.6f - 0.4f * br, 0.4f - 0.3f * br, 0.75f);
    /* batterie de la lampe */
    float bat = P.batterie / LAMPE_BATTERIE;
    int faible = bat < LAMPE_FAIBLE;
    if (!faible || fmodf(t, 0.8f) < 0.5f) {
        rect(winW - 92.0f, winH - 44.0f, 60, 12, 0.6f, 0.6f, 0.55f, 0.35f);
        rect(winW - 32.0f, winH - 41.0f, 4, 6, 0.6f, 0.6f, 0.55f, 0.35f);
        rect(winW - 90.0f, winH - 42.0f, 56 * bat, 8, faible ? 0.9f : 0.75f, faible ? 0.2f : 0.72f, faible ? 0.1f : 0.5f, 0.7f);
    }
    /* endurance : seulement quand elle n'est pas pleine */
    float e = P.endurance / ENDURANCE_MAX;
    if (e < 0.999f) rect(winW * 0.5f - 60, winH - 30.0f, 120 * e, 3, P.essouffle ? 0.8f : 0.85f, P.essouffle ? 0.2f : 0.8f,
                         P.essouffle ? 0.15f : 0.6f, 0.5f);
    if (fusible_vise()) text_c(0, winW * 0.5f, winH * 0.5f + 60, "[E]  ramasser le fusible", 0.85f, 0.82f, 0.7f, 0.8f);
    if (msgT > 0) text_c(1, winW * 0.5f, winH - 90.0f, msgTexte, 0.92f, 0.88f, 0.72f, fminf(1, msgT));
    if (carteNiveauT > 0) {                               /* carton d'entree */
        float a = fminf(1, carteNiveauT / 1.5f) * fminf(1, (6.0f - carteNiveauT) * 2);
        snprintf(b, sizeof b, "NIVEAU %d", niveauNum - 1);
        text_c(1, winW * 0.5f, winH * 0.36f, b, 0.8f, 0.76f, 0.6f, a);
        text_c(2, winW * 0.5f, winH * 0.36f + 64, niveau_nom(niveauNum), 0.95f, 0.9f, 0.62f, a);
    }
}

static void render(float alpha, float frameDt) {
    (void)frameDt;
    float t = appState == APP_GAME ? gameT : (float)nowT;
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    V3 eye;
    float yaw, pitch, roll = 0, fov = FOV_V;
    if (appState == APP_GAME) {
        eye = player_eye(alpha, &roll);
        yaw = P.yaw; pitch = P.pitch;
        fov += FOV_COURSE * P.courseK;
        if (tension > 0.6f) { roll += sinf(t * 2.1f) * (tension - 0.6f) * 2; }   /* poursuite : la vue tangue */
        lampeK = lampe_intensite(t);
    } else {                                              /* menu : on regarde le hall, sans lampe */
        eye = v3(carteDepart.x, 1.6f, carteDepart.z);
        yaw = carteDepartYaw + sinf(t * 0.07f) * 0.5f; pitch = -0.05f + sinf(t * 0.05f) * 0.04f;
        lampeK = 0;
    }
    if (screamT >= 0) {                                   /* screamer : tout tremble */
        float s = 1 - fminf(1, screamT / SCREAMER_MORT_T);
        yaw += sinf(t * 83) * 0.05f * s; pitch += cosf(t * 71) * 0.05f * s; roll += sinf(t * 97) * 6 * s;
    }
    camEye = eye;
    set_camera(eye, yaw, pitch, roll, fov);
    glEnable(GL_DEPTH_TEST);
    float dy = P.lampeYaw - P.yaw;
    while (dy > PI) dy -= 2 * PI;
    while (dy < -PI) dy += 2 * PI;
    monde_draw(t, appState == APP_GAME ? lampeK : 0, dy, P.lampePitch - P.pitch);
    if (appState == APP_GAME) dessiner_entites(t);

    if (screamT >= 0) {                                   /* la tete, devant toi */
        begin_2d();                                       /* le monde s'efface dans un rouge sale */
        rect(0, 0, (float)winW, (float)winH, 0.08f, 0.0f, 0.0f, 0.88f);
        glEnable(GL_DEPTH_TEST); glDisable(GL_BLEND);
        glClear(GL_DEPTH_BUFFER_BIT);
        set_camera(v3(0, 0, 0), 0, 0, sinf(t * 97) * 4, FOV_V);
        dessiner_screamer(screamT);
    }

    begin_2d();
    vignette(0.55f + 0.3f * tension);
    if (appState == APP_MENU) draw_title(t);
    else {
        if (tension > 0.3f) {                             /* poursuite : les bords battent en rouge sombre */
            float p = (tension - 0.3f) * (0.5f + 0.5f * sinf(t * 7));
            rect_vgrad(0, 0, (float)winW, winH * 0.2f, 0.35f, 0, 0, p * 0.35f, 0);
            rect_vgrad(0, winH * 0.8f, (float)winW, winH * 0.2f, 0.35f, 0, 0, 0, p * 0.35f);
        }
        if (screamT >= 0) {                               /* flashs blanc / rouge, puis noir */
            float f = fmodf(screamT, 0.1f) < 0.05f ? 1.0f : 0.0f;
            if (screamT < 0.6f) rect(0, 0, (float)winW, (float)winH, 0.55f * f + 0.05f, 0, 0, 0.35f);   /* flashs rouge sang */
            if (screamT > SCREAMER_MORT_T - 0.5f) rect(0, 0, (float)winW, (float)winH, 0, 0, 0, (screamT - (SCREAMER_MORT_T - 0.5f)) * 2);
        } else if (mortT >= 0) {
            rect(0, 0, (float)winW, (float)winH, 0, 0, 0, 1);
            float a = fminf(1, mortT);
            text_c(2, winW * 0.5f, winH * 0.42f, "IL T'A ENTENDU.", 0.75f, 0.08f, 0.05f, a);
            if (mortT > 1.2f) text_c(0, winW * 0.5f, winH * 0.42f + 60, "[E] recommencer le niveau", 0.6f, 0.55f, 0.5f, a * 0.8f);
        } else {
            draw_vhs_overlay(t);
            draw_hud(t);
            if (sortieT > ASC_PORTE_T) rect(0, 0, (float)winW, (float)winH, 0, 0, 0, fminf(1, (sortieT - ASC_PORTE_T) / 1.5f));
            if (!captured && !shotMode) draw_pause();
        }
    }
    glEnable(GL_DEPTH_TEST);
}
