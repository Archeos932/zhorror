/* BOURDONNEMENT - render() : l'image complete (salle de test de l'etape 0, puis menus)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   Etape 0 : couleurs par sommet et brouillard, pas encore de textures ni de lightmaps (etapes 1 et 3). */

/* eclairement d'un point : somme des neons allumes (atténuation douce), pour colorer les sommets */
static float salle_lumiere(float x, float y, float z, float t) {
    float l = 0.08f;                                      /* ambiance minimale */
    for (int i = 0; i < NEON_N * NEON_N; i++) {
        if (!neon_allume(i, t)) continue;
        float nx = -SALLE_DEMI + NEON_PAS * (i % NEON_N + 0.5f), nz = -SALLE_DEMI + NEON_PAS * (i / NEON_N + 0.5f);
        float dx = x - nx, dy = y - SALLE_HAUT, dz = z - nz;
        l += 0.9f / (1.0f + 0.12f * (dx * dx + dy * dy + dz * dz));
    }
    return fminf(l, 1.35f);
}

/* quad subdivise (n x n) entre a, a+u, a+u+v, a+v, couleur de base c, eclaire par sommet */
static void quad_lit(V3 a, V3 u, V3 v, V3 c, int n, float t) {
    for (int j = 0; j < n; j++) {
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= n; i++) {
            for (int k = 0; k < 2; k++) {
                float fu = (float)i / n, fv = (float)(j + k) / n;
                V3 p = vadd(a, vadd(vmul(u, fu), vmul(v, fv)));
                float l = salle_lumiere(p.x, p.y, p.z, t);
                glColor3f(c.x * l, c.y * l, c.z * l);
                glVertex3f(p.x, p.y, p.z);
            }
        }
        glEnd();
    }
}

static void boite_lit(float x0, float z0, float x1, float z1, float h, V3 c, float t) {
    quad_lit(v3(x0, 0, z1), v3(x1 - x0, 0, 0), v3(0, h, 0), c, 4, t);   /* face +z */
    quad_lit(v3(x1, 0, z0), v3(x0 - x1, 0, 0), v3(0, h, 0), c, 4, t);   /* face -z */
    quad_lit(v3(x0, 0, z0), v3(0, 0, z1 - z0), v3(0, h, 0), c, 4, t);   /* face -x */
    quad_lit(v3(x1, 0, z1), v3(0, 0, z0 - z1), v3(0, h, 0), c, 4, t);   /* face +x */
    quad_lit(v3(x0, h, z0), v3(x1 - x0, 0, 0), v3(0, 0, z1 - z0), vmul(c, 0.9f), 2, t);   /* dessus */
}

static void draw_salle(float t) {
    float D = SALLE_DEMI, H = SALLE_HAUT;
    V3 moquette = v3(0.55f, 0.48f, 0.26f), mur = v3(0.86f, 0.79f, 0.47f), plafond = v3(0.50f, 0.48f, 0.40f);
    quad_lit(v3(-D, 0, -D), v3(0, 0, 2 * D), v3(2 * D, 0, 0), moquette, 20, t);   /* sol */
    quad_lit(v3(-D, H, -D), v3(2 * D, 0, 0), v3(0, 0, 2 * D), plafond, 20, t);    /* plafond */
    quad_lit(v3(-D, 0, -D), v3(2 * D, 0, 0), v3(0, H, 0), mur, 16, t);           /* 4 murs */
    quad_lit(v3(D, 0, D), v3(-2 * D, 0, 0), v3(0, H, 0), mur, 16, t);
    quad_lit(v3(-D, 0, D), v3(0, 0, -2 * D), v3(0, H, 0), mur, 16, t);
    quad_lit(v3(D, 0, -D), v3(0, 0, 2 * D), v3(0, H, 0), mur, 16, t);
    for (int i = 0; i < NPILIERS; i++) {
        const Pilier *p = &piliers[i];
        float h = (i == 4) ? 1.1f : H;                    /* la cloison du milieu est basse */
        boite_lit(p->x0, p->z0, p->x1, p->z1, h, mur, t);
    }
    /* panneaux de neons (emissifs : pas d'eclairage) */
    for (int i = 0; i < NEON_N * NEON_N; i++) {
        float nx = -D + NEON_PAS * (i % NEON_N + 0.5f), nz = -D + NEON_PAS * (i / NEON_N + 0.5f), y = H - 0.01f;
        float on = neon_allume(i, t) ? 1.0f : 0.25f;
        glColor3f(1.0f * on, 0.98f * on, 0.86f * on);
        glBegin(GL_QUADS);
        glVertex3f(nx - 0.6f, y, nz - 0.3f); glVertex3f(nx + 0.6f, y, nz - 0.3f);
        glVertex3f(nx + 0.6f, y, nz + 0.3f); glVertex3f(nx - 0.6f, y, nz + 0.3f);
        glEnd();
    }
}

static void render(float alpha, float frameDt) {
    (void)frameDt;
    float t = appState == APP_GAME ? gameT : (float)nowT;
    glClearColor(0.05f, 0.045f, 0.02f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    V3 eye;
    float yaw, pitch;
    if (appState == APP_GAME) {
        eye = vlerp(P.prevPos, P.pos, alpha);
        eye.y = player_eye_y(alpha);
        yaw = P.yaw; pitch = P.pitch;
    } else {                                              /* menu : lente derive dans la salle */
        eye = v3(2.5f + sinf(t * 0.05f) * 0.8f, 1.6f, 8.8f);
        yaw = -0.35f + sinf(t * 0.07f) * 0.12f; pitch = -0.05f;
    }
    set_camera(eye, yaw, pitch);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D); glDisable(GL_BLEND);
    glEnable(GL_FOG);
    {
        GLfloat fc[4] = { 0.10f, 0.09f, 0.04f, 1 };
        glFogi(GL_FOG_MODE, GL_EXP2);
        glFogf(GL_FOG_DENSITY, 0.075f);
        glFogfv(GL_FOG_COLOR, fc);
    }
    draw_salle(t);

    begin_2d();
    rect_vgrad(0, 0, (float)winW, winH * 0.25f, 0, 0, 0, 0.45f, 0);   /* vignette (en attendant le filtre VHS) */
    rect_vgrad(0, winH * 0.75f, (float)winW, winH * 0.25f, 0, 0, 0, 0, 0.45f);
    if (appState == APP_MENU) draw_title(t);
    else {
        draw_vhs_overlay(t);
        if (!captured && !shotMode) draw_pause();
        else {                                            /* endurance : seulement quand elle n'est pas pleine */
            float e = P.endurance / ENDURANCE_MAX;
            if (e < 0.999f) rect(winW * 0.5f - 60, winH - 30.0f, 120 * e, 3, P.essouffle ? 0.8f : 0.85f,
                                 P.essouffle ? 0.2f : 0.8f, P.essouffle ? 0.15f : 0.6f, 0.5f);
        }
    }
    glEnable(GL_DEPTH_TEST);
}
