/* BOURDONNEMENT - dessin des meubles : des boites eclairees par lum_entite (neons, lampes, lampe torche),
   comme les monstres. Repere d'un meuble : origine au sol, le devant regarde vers -z.
   Morceau de main.c : inclus dans l'ordre (apres rendu/entites.c), ne se compile pas seul. */

/* une boite posee : centre (x, y, z) dans le repere du meuble, demi-tailles, couleur x lumiere */
static void bp(float x, float y, float z, float hx, float hy, float hz, float r, float g, float b, float l) {
    glPushMatrix(); glTranslatef(x, y, z); boite(hx, hy, hz, r * l, g * l, b * l); glPopMatrix();
}
static void bp_emissif(float x, float y, float z, float hx, float hy, float hz, float r, float g, float b) {
    glDisable(GL_FOG);
    glPushMatrix(); glTranslatef(x, y, z); boite(hx, hy, hz, r, g, b); glPopMatrix();
    glEnable(GL_FOG);
}

/* ---- videosurveillance : la vue de chaque camera, rendue dans une petite texture ---------------------- */
#define CAMTEX 128
static GLuint texCam[MAXCAMERAS];
static int camTour;
static float camSalleT;                                   /* temps passe dans la salle des cameras (la camera "derriere toi") */

/* l'ecran k : l'image de la camera (teinte verdatre de moniteur) ou de la neige */
static void ecran(int k, float x0, float y0, float w, float h, float z, float t) {
    glDisable(GL_FOG);
    if (ncameras > 0 && texCam[k % ncameras] && glReady) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texCam[k % ncameras]);
        float fl = 0.85f + 0.15f * ((hash32((unsigned)(t * 20) + (unsigned)k) & 7) == 0 ? 0.0f : 1.0f);
        glColor3f(0.75f * fl, 0.95f * fl, 0.8f * fl);
        glBegin(GL_QUADS);                                /* image a l'endroit, vue de face (-z) */
        glTexCoord2f(1, 0); glVertex3f(x0, y0, z); glTexCoord2f(0, 0); glVertex3f(x0 + w, y0, z);
        glTexCoord2f(0, 1); glVertex3f(x0 + w, y0 + h, z); glTexCoord2f(1, 1); glVertex3f(x0, y0 + h, z);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    } else {
        glBegin(GL_QUADS);
        unsigned s = (unsigned)(t * 25);
        for (int j = 0; j < 6; j++)
            for (int i = 0; i < 8; i++) {
                float c = 0.1f + 0.4f * ((hash32(s * 31u + (unsigned)(k * 97 + j * 8 + i)) & 255) / 255.0f);
                glColor3f(c * 0.8f, c, c * 0.85f);
                float a = x0 + w * i / 8, b = y0 + h * j / 6;
                glVertex3f(a, b, z); glVertex3f(a + w / 8, b, z); glVertex3f(a + w / 8, b + h / 6, z); glVertex3f(a, b + h / 6, z);
            }
        glEnd();
    }
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);   /* lignes de balayage */
    glColor4f(0, 0, 0, 0.35f);
    glBegin(GL_QUADS);
    for (int j = 0; j < 14; j++) { float b = y0 + h * j / 14; glVertex3f(x0, b, z - 0.001f); glVertex3f(x0 + w, b, z - 0.001f); glVertex3f(x0 + w, b + h / 40, z - 0.001f); glVertex3f(x0, b + h / 40, z - 0.001f); }
    glEnd();
    glDisable(GL_BLEND);
    glEnable(GL_FOG);
}

static void lit(float l, int petit, int forme) {         /* forme : quelque chose sous les draps */
    float k = petit ? 0.65f : 1.0f, hx = 0.7f * k, hz = 1.0f * (petit ? 0.7f : 1.0f);
    for (int s = -1; s <= 1; s += 2)
        for (int t = -1; t <= 1; t += 2) bp(s * (hx - 0.05f), 0.08f, t * (hz - 0.05f), 0.035f, 0.08f, 0.035f, 0.28f, 0.18f, 0.1f, l);
    bp(0, 0.24f, 0, hx, 0.08f, hz, 0.3f, 0.2f, 0.12f, l);                       /* sommier */
    bp(0, 0.38f, 0.02f, hx - 0.03f, 0.07f, hz - 0.04f, 0.72f, 0.69f, 0.6f, l);  /* matelas sale */
    bp(0, 0.5f, hz - 0.22f, hx * 0.7f, 0.05f, 0.14f, 0.82f, 0.8f, 0.72f, l);   /* oreiller */
    float cr = petit ? 0.22f : 0.42f, cg = petit ? 0.3f : 0.1f, cb = petit ? 0.5f : 0.11f;
    bp(0, 0.47f, -0.25f * k, hx + 0.02f, 0.035f, hz * 0.72f, cr, cg, cb, l);  /* couverture */
    if (forme) { bp(0.05f, 0.56f, -0.1f, 0.2f * k, 0.1f, 0.55f * k, cr * 0.9f, cg * 0.9f, cb * 0.9f, l); bp(0.05f, 0.58f, 0.45f * k, 0.13f, 0.08f, 0.12f, cr, cg, cb, l); }
    bp(0, 0.5f, hz, hx + 0.03f, 0.5f * k, 0.04f, 0.26f, 0.16f, 0.09f, l);     /* tete de lit */
}

static void tele(float l, int etat, float t) {
    bp(0, 0.25f, 0, 0.4f, 0.25f, 0.24f, 0.22f, 0.14f, 0.08f, l);                /* meuble */
    bp(0, 0.7f, 0.03f, 0.3f, 0.2f, 0.2f, 0.16f, 0.15f, 0.14f, l);               /* poste a tube */
    glDisable(GL_FOG);
    glBegin(GL_QUADS);                                    /* l'ecran : neige, noir, ou un visage */
    float x0 = -0.24f, y0 = 0.56f, w = 0.48f, h = 0.3f, z = -0.172f;
    unsigned s = (unsigned)(t * 30);
    for (int j = 0; j < 9; j++)
        for (int i = 0; i < 12; i++) {
            float c = 0.02f;
            if (etat == 1) c = 0.15f + 0.55f * ((hash32(s * 977u + (unsigned)(j * 12 + i)) & 255) / 255.0f);
            if (etat == 2) {
                float u = (i + 0.5f) / 12 - 0.5f, v = (j + 0.5f) / 9 - 0.5f;
                c = (u * u * 2.2f + v * v * 1.2f < 0.18f) ? 0.85f : 0.05f;            /* le visage */
                if ((fabsf(u + 0.12f) < 0.05f || fabsf(u - 0.12f) < 0.05f) && fabsf(v - 0.08f) < 0.06f) c = 0;   /* les yeux */
                if (fabsf(u) < 0.14f && fabsf(v + 0.17f) < 0.03f) c = 0;   /* la bouche */
            }
            glColor3f(c, c, c * 1.05f);
            float a = x0 + w * i / 12, b = y0 + h * j / 9;
            glVertex3f(a, b, z); glVertex3f(a + w / 12, b, z); glVertex3f(a + w / 12, b + h / 9, z); glVertex3f(a, b + h / 9, z);
        }
    glEnd();
    glEnable(GL_FOG);
}

static void dessiner_meuble(const Meuble *m, float t) {
    const MeubleDef *d = &meubleDef[m->type];
    float l = lum_entite(v3(m->x, fmaxf(0.3f, d->h * 0.6f), m->z), t);
    float on = m->lumiere >= 0 && neon_allume(m->lumiere, t) ? 1.0f : 0.0f;
    glPushMatrix(); glTranslatef(m->x, 0, m->z); glRotatef(-m->yaw * 180.0f / PI, 0, 1, 0);
    switch (m->type) {
    case MB_LIT: lit(l, 0, m->etat); break;
    case MB_PETIT_LIT: lit(l, 1, 0); break;
    case MB_ARMOIRE:
        bp(0, 1.0f, 0, 0.5f, 1.0f, 0.3f, 0.27f, 0.17f, 0.1f, l);
        bp(0, 1.0f, -0.302f, 0.005f, 0.95f, 0.002f, 0.1f, 0.06f, 0.04f, l);   /* fente entre les portes */
        for (int s = -1; s <= 1; s += 2) bp(s * 0.06f, 1.05f, -0.31f, 0.012f, 0.06f, 0.01f, 0.6f, 0.55f, 0.4f, l);
        if (m->etat) {                                    /* porte entrouverte : du noir dedans */
            glPushMatrix(); glTranslatef(0.48f, 1.0f, -0.3f); glRotatef(-35, 0, 1, 0);
            boite(0.24f, 0.95f, 0.02f, 0.25f * l, 0.16f * l, 0.09f * l); glPopMatrix();
            bp(0.24f, 1.0f, -0.28f, 0.22f, 0.94f, 0.005f, 0.01f, 0.01f, 0.01f, 1);
        }
        break;
    case MB_CHEVET:
        bp(0, 0.25f, 0, 0.22f, 0.25f, 0.2f, 0.3f, 0.19f, 0.11f, l);
        bp(0, 0.32f, -0.202f, 0.18f, 0.005f, 0.002f, 0.12f, 0.07f, 0.04f, l);
        break;
    case MB_LAMPE:
        bp(0, 0.54f, 0, 0.05f, 0.04f, 0.05f, 0.5f, 0.45f, 0.3f, l);
        bp(0, 0.66f, 0, 0.015f, 0.1f, 0.015f, 0.5f, 0.45f, 0.3f, l);
        if (on > 0) bp_emissif(0, 0.82f, 0, 0.12f, 0.08f, 0.12f, 1.0f, 0.78f, 0.45f);   /* abat-jour qui brille */
        else bp(0, 0.82f, 0, 0.12f, 0.08f, 0.12f, 0.6f, 0.5f, 0.35f, l);
        break;
    case MB_TELE: tele(l, m->etat, t); break;
    case MB_CHAISE:
        for (int s = -1; s <= 1; s += 2) for (int u = -1; u <= 1; u += 2) bp(s * 0.18f, 0.22f, u * 0.18f, 0.02f, 0.22f, 0.02f, 0.25f, 0.16f, 0.09f, l);
        bp(0, 0.46f, 0, 0.21f, 0.025f, 0.21f, 0.3f, 0.2f, 0.11f, l);
        bp(0, 0.7f, 0.19f, 0.21f, 0.22f, 0.02f, 0.3f, 0.2f, 0.11f, l);
        break;
    case MB_TABLE:
        if (m->etat) { glTranslatef(0, 0.4f, 0); glRotatef(88, 1, 0, 0); glTranslatef(0, -0.37f, 0); }   /* renversee */
        bp(0, 0.73f, 0, 0.55f, 0.025f, 0.4f, 0.4f, 0.28f, 0.16f, l);
        for (int s = -1; s <= 1; s += 2) for (int u = -1; u <= 1; u += 2) bp(s * 0.5f, 0.36f, u * 0.35f, 0.025f, 0.36f, 0.025f, 0.3f, 0.2f, 0.11f, l);
        break;
    case MB_LAVABO:
        bp(0, 0.4f, 0.1f, 0.07f, 0.4f, 0.07f, 0.85f, 0.85f, 0.82f, l);
        bp(0, 0.82f, 0, 0.3f, 0.06f, 0.22f, 0.86f, 0.86f, 0.83f, l);
        bp(0, 0.875f, -0.02f, 0.22f, 0.01f, 0.15f, 0.2f, 0.18f, 0.16f, l);       /* le bac */
        bp(0, 1.45f, 0.215f, 0.3f, 0.35f, 0.006f, 0.08f + 0.06f * l, 0.09f + 0.06f * l, 0.1f + 0.07f * l, 1);   /* miroir : noir luisant */
        bp(0, 1.45f, 0.222f, 0.32f, 0.37f, 0.004f, 0.5f, 0.48f, 0.42f, l);
        break;
    case MB_BAIGNOIRE: {
        bp(0, 0.28f, 0, 0.4f, 0.28f, 0.85f, 0.84f, 0.84f, 0.8f, l);
        bp(0, 0.5f, 0, 0.32f, 0.01f, 0.76f, 0.02f, 0.035f, 0.03f, 1);             /* eau noire */
        float s = m->px[0] ? m->px[0] : 1;                /* cote de la piece : rideau de douche */
        bp(s * 0.4f, 2.0f, 0, 0.01f, 0.01f, 0.85f, 0.6f, 0.6f, 0.6f, l);
        if (m->etat == 1) bp(s * 0.25f, 1.05f, 0.2f, 0.12f, 0.6f, 0.1f, 0.03f, 0.03f, 0.03f, 1);   /* quelqu'un, derriere */
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
        glColor4f(0.7f * l, 0.72f * l, 0.68f * l, 0.72f);
        glBegin(GL_QUADS);
        for (int k = 0; k < 10; k++) {                    /* rideau ondule */
            float z0 = -0.85f + k * 0.17f, z1 = z0 + 0.17f, dx = (k & 1 ? 0.04f : -0.02f);
            glVertex3f(s * 0.4f + dx, 0.6f, z0); glVertex3f(s * 0.4f - dx, 0.6f, z1); glVertex3f(s * 0.4f - dx, 1.98f, z1); glVertex3f(s * 0.4f + dx, 1.98f, z0);
        }
        glEnd();
        glDepthMask(GL_TRUE); glDisable(GL_BLEND);
        break;
    }
    case MB_TOILETTES:
        bp(0, 0.2f, -0.05f, 0.18f, 0.2f, 0.22f, 0.85f, 0.85f, 0.82f, l);
        bp(0, 0.41f, -0.07f, 0.19f, 0.02f, 0.23f, 0.7f, 0.68f, 0.6f, l);
        bp(0, 0.55f, 0.24f, 0.2f, 0.2f, 0.07f, 0.85f, 0.85f, 0.82f, l);
        break;
    case MB_JOUETS:
        bp(-0.1f, 0.05f, 0.05f, 0.05f, 0.05f, 0.05f, 0.7f, 0.15f, 0.12f, l);
        bp(0.08f, 0.05f, -0.06f, 0.05f, 0.05f, 0.05f, 0.15f, 0.3f, 0.7f, l);
        bp(0.1f, 0.13f, -0.04f, 0.04f, 0.04f, 0.04f, 0.8f, 0.7f, 0.2f, l);
        bp(-0.12f, 0.12f, -0.15f, 0.08f, 0.12f, 0.06f, 0.42f, 0.27f, 0.15f, l);   /* nounours */
        bp(-0.12f, 0.29f, -0.15f, 0.06f, 0.06f, 0.05f, 0.42f, 0.27f, 0.15f, l);
        bp(-0.12f, 0.29f, -0.205f, 0.012f, 0.012f, 0.005f, 0, 0, 0, 1);
        break;
    case MB_BOITE_MUSIQUE:
        bp(0, 0.55f, 0, 0.1f, 0.05f, 0.08f, 0.5f, 0.3f, 0.35f, l);
        glPushMatrix(); glTranslatef(0, 0.6f, 0.08f); glRotatef(-70, 1, 0, 0); boite(0.1f, 0.005f, 0.08f, 0.5f * l, 0.3f * l, 0.35f * l); glPopMatrix();
        glPushMatrix(); glTranslatef(0, 0.66f, 0); glRotatef(t * 60, 0, 1, 0);   /* la danseuse tourne */
        boite(0.012f, 0.05f, 0.012f, 0.9f * l, 0.8f * l, 0.8f * l); glPopMatrix();
        if (on > 0) bp_emissif(0.14f, 0.53f, 0, 0.02f, 0.02f, 0.02f, 0.9f, 0.85f, 0.5f);   /* veilleuse */
        break;
    case MB_FRIGO: {
        bp(0, 0.9f, 0, 0.35f, 0.9f, 0.33f, 0.78f, 0.76f, 0.64f, l);
        float a = fminf(1, m->etat / 100.0f) * 95;        /* etat : ouverture (0..100) */
        glPushMatrix(); glTranslatef(0.35f, 0.9f, -0.33f); glRotatef(a, 0, 1, 0); glTranslatef(-0.35f, 0, -0.02f);
        boite(0.35f, 0.88f, 0.02f, 0.8f * l, 0.78f * l, 0.66f * l);
        glTranslatef(-0.28f, 0.2f, -0.03f); boite(0.015f, 0.2f, 0.015f, 0.4f * l, 0.4f * l, 0.4f * l);
        glPopMatrix();
        if (a > 5) bp_emissif(0, 0.9f, -0.2f, 0.3f, 0.82f, 0.1f, 0.55f, 0.6f, 0.5f);   /* la lumiere du frigo */
        break;
    }
    case MB_COMPTOIR:
        bp(0, 0.43f, 0, 0.5f, 0.43f, 0.3f, 0.5f, 0.4f, 0.26f, l);
        bp(0, 0.88f, -0.02f, 0.52f, 0.02f, 0.32f, 0.6f, 0.58f, 0.5f, l);
        bp(0, 0.43f, -0.302f, 0.005f, 0.4f, 0.002f, 0.2f, 0.15f, 0.1f, l);
        break;
    case MB_RAYONNAGE:
        for (int s = -1; s <= 1; s += 2) bp(s * 0.48f, 1.0f, 0, 0.02f, 1.0f, 0.24f, 0.35f, 0.36f, 0.38f, l);
        for (int k = 0; k < 4; k++) {
            bp(0, 0.1f + k * 0.6f, 0, 0.48f, 0.015f, 0.24f, 0.4f, 0.41f, 0.43f, l);
            if (k < 3 && (hash32((unsigned)(m->x * 31 + m->z * 17) + (unsigned)k) & 3)) bp(((int)(k * 37) % 5 - 2) * 0.1f, 0.3f + k * 0.6f, 0, 0.18f, 0.18f, 0.18f, 0.45f, 0.33f, 0.2f, l);
        }
        break;
    case MB_CARTONS:
        bp(0, 0.2f, 0, 0.3f, 0.2f, 0.28f, 0.48f, 0.35f, 0.2f, l);
        bp(0.05f, 0.52f, 0.02f, 0.22f, 0.12f, 0.2f, 0.44f, 0.32f, 0.18f, l);
        glPushMatrix(); glTranslatef(-0.08f, 0.71f, -0.05f); glRotatef(18, 0, 1, 0); boite(0.14f, 0.07f, 0.12f, 0.5f * l, 0.37f * l, 0.21f * l); glPopMatrix();
        break;
    case MB_MANNEQUIN:
        bp(0, 0.02f, 0, 0.18f, 0.02f, 0.18f, 0.2f, 0.2f, 0.2f, l);
        bp(0, 0.5f, 0, 0.02f, 0.48f, 0.02f, 0.3f, 0.3f, 0.3f, l);
        bp(0, 1.2f, 0, 0.19f, 0.32f, 0.11f, 0.72f, 0.66f, 0.6f, l);      /* buste lisse */
        for (int s = -1; s <= 1; s += 2) bp(s * 0.23f, 1.12f, 0, 0.04f, 0.32f, 0.04f, 0.7f, 0.64f, 0.58f, l);
        bp(0, 1.62f, 0, 0.09f, 0.11f, 0.1f, 0.74f, 0.68f, 0.62f, l);     /* tete sans visage */
        break;
    case MB_SAC_COUCHAGE:
        bp(0, 0.07f, 0, 0.38f, 0.07f, 0.92f, 0.16f, 0.24f, 0.14f, l);
        bp(0, 0.12f, 0.7f, 0.3f, 0.06f, 0.16f, 0.5f, 0.48f, 0.4f, l);
        break;
    case MB_LANTERNE:
        bp(0, 0.03f, 0, 0.07f, 0.03f, 0.07f, 0.2f, 0.2f, 0.2f, l);
        if (on > 0) bp_emissif(0, 0.15f, 0, 0.055f, 0.09f, 0.055f, 1.0f, 0.75f, 0.35f);
        else bp(0, 0.15f, 0, 0.055f, 0.09f, 0.055f, 0.3f, 0.25f, 0.15f, l);
        bp(0, 0.27f, 0, 0.07f, 0.02f, 0.07f, 0.2f, 0.2f, 0.2f, l);
        break;
    case MB_BUREAU: {
        unsigned h = hash32((unsigned)(m->x * 97 + m->z * 13));
        bp(0, 0.735f, 0, 0.6f, 0.025f, 0.35f, 0.42f, 0.36f, 0.26f, l);
        for (int s = -1; s <= 1; s += 2) bp(s * 0.57f, 0.36f, 0, 0.02f, 0.36f, 0.33f, 0.35f, 0.3f, 0.22f, l);
        bp(0.35f, 0.5f, 0, 0.2f, 0.22f, 0.32f, 0.38f, 0.33f, 0.24f, l);   /* caisson */
        bp(-0.1f, 0.94f, 0.12f, 0.19f, 0.17f, 0.17f, 0.6f, 0.58f, 0.5f, l);   /* vieil ecran a tube */
        bp(-0.1f, 0.94f, -0.052f, 0.15f, 0.12f, 0.002f, 0.03f, 0.04f, 0.035f, 1);
        bp(-0.1f, 0.77f, -0.18f, 0.2f, 0.012f, 0.07f, 0.55f, 0.53f, 0.46f, l);   /* clavier */
        if (h & 1) bp(0.35f, 0.77f, -0.1f, 0.12f, 0.01f, 0.16f, 0.8f, 0.78f, 0.7f, l);   /* une pile de feuilles */
        if (h & 2) bp(-0.45f, 0.8f, -0.15f, 0.04f, 0.05f, 0.04f, 0.7f, 0.2f, 0.15f, l);   /* un mug */
        break;
    }
    case MB_CHAISE_BUREAU:
        for (int k = 0; k < 4; k++) { glPushMatrix(); glRotatef(k * 90.0f + 45, 0, 1, 0); bp(0, 0.04f, 0.15f, 0.025f, 0.02f, 0.15f, 0.12f, 0.12f, 0.12f, l); glPopMatrix(); }
        bp(0, 0.25f, 0, 0.025f, 0.22f, 0.025f, 0.2f, 0.2f, 0.2f, l);
        bp(0, 0.48f, 0, 0.23f, 0.04f, 0.23f, 0.15f, 0.17f, 0.25f, l);
        bp(0, 0.78f, 0.2f, 0.21f, 0.25f, 0.035f, 0.15f, 0.17f, 0.25f, l);
        break;
    case MB_CLASSEUR: {
        int ouvert = (int)(hash32((unsigned)(m->x * 31 + m->z * 7)) % 4);
        bp(0, 0.65f, 0, 0.25f, 0.65f, 0.3f, 0.45f, 0.47f, 0.45f, l);
        for (int k = 0; k < 4; k++) {
            float y = 0.18f + k * 0.31f, sort = k == ouvert ? 0.3f : 0;
            bp(0, y, -0.3f - sort * 0.5f, 0.23f, 0.14f, 0.005f + sort * 0.5f, 0.5f, 0.52f, 0.5f, l);
            bp(0, y + 0.06f, -0.31f - sort, 0.06f, 0.012f, 0.012f, 0.25f, 0.25f, 0.25f, l);
        }
        break;
    }
    case MB_FONTAINE:
        bp(0, 0.5f, 0, 0.17f, 0.5f, 0.17f, 0.82f, 0.8f, 0.74f, l);
        bp(0, 0.85f, -0.172f, 0.04f, 0.04f, 0.005f, 0.2f, 0.25f, 0.6f, l);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glPushMatrix(); glTranslatef(0, 1.18f, 0); boite(0.13f, 0.17f, 0.13f, 0.3f * l, 0.45f * l, 0.65f * l); glPopMatrix();   /* bonbonne */
        glDisable(GL_BLEND);
        break;
    case MB_PAPIERS: {
        unsigned h = hash32((unsigned)(m->x * 131 + m->z * 71));
        glBegin(GL_QUADS);
        for (int k = 0; k < 5; k++) {
            float px = ((h >> (k * 3)) % 60) / 100.0f - 0.3f, pz = ((h >> (k * 3 + 1)) % 60) / 100.0f - 0.3f, a = k * 1.3f + (h % 7);
            float ca = cosf(a) * 0.105f, sa = sinf(a) * 0.105f, cb = cosf(a) * 0.148f, sb = sinf(a) * 0.148f, y = 0.004f + k * 0.0008f;
            float g = 0.82f * l * (k & 1 ? 0.92f : 1.0f);
            glColor3f(g, g * 0.97f, g * 0.86f);
            glVertex3f(px - ca + sb, y, pz - sa - cb); glVertex3f(px + ca + sb, y, pz + sa - cb);
            glVertex3f(px + ca - sb, y, pz + sa + cb); glVertex3f(px - ca - sb, y, pz - sa + cb);
        }
        glEnd();
        break;
    }
    case MB_PLANTE:
        bp(0, 0.17f, 0, 0.16f, 0.17f, 0.16f, 0.4f, 0.24f, 0.14f, l);
        bp(0, 0.33f, 0, 0.14f, 0.01f, 0.14f, 0.15f, 0.1f, 0.06f, l);
        for (int k = 0; k < 7; k++) {                     /* feuilles mortes, tombantes */
            glPushMatrix(); glTranslatef(0, 0.34f, 0); glRotatef(k * 51.0f, 0, 1, 0); glRotatef(28 + (k % 3) * 18.0f, 1, 0, 0);
            glTranslatef(0, 0.3f, 0); boite(0.03f, 0.3f, 0.008f, 0.33f * l, 0.27f * l, 0.12f * l); glPopMatrix();
        }
        break;
    case MB_ECRANS:
        bp(0, 0.735f, -0.1f, 0.8f, 0.025f, 0.3f, 0.3f, 0.3f, 0.32f, l);
        for (int s = -1; s <= 1; s += 2) bp(s * 0.77f, 0.36f, -0.1f, 0.02f, 0.36f, 0.28f, 0.25f, 0.25f, 0.27f, l);
        for (int j = 0; j < 2; j++)                       /* 2 x 2 moniteurs */
            for (int i = 0; i < 2; i++) {
                float x = -0.38f + i * 0.76f, y = 1.0f + j * 0.42f;
                bp(x, y, 0.15f, 0.34f, 0.2f, 0.16f, 0.18f, 0.18f, 0.2f, l);
                ecran(j * 2 + i, x - 0.29f, y - 0.16f, 0.58f, 0.32f, -0.012f, t);
            }
        bp(0, 0.77f, -0.25f, 0.22f, 0.012f, 0.07f, 0.4f, 0.4f, 0.42f, l);
        break;
    case MB_CAMERA: {
        glTranslatef(0, HAUT_PLAFOND - 0.25f, 0);
        bp(0, 0.13f, 0.05f, 0.02f, 0.11f, 0.02f, 0.3f, 0.3f, 0.3f, l);   /* support */
        glRotatef(18, 1, 0, 0);
        bp(0, 0, 0, 0.06f, 0.05f, 0.11f, 0.75f, 0.74f, 0.7f, l);
        bp(0, 0, -0.115f, 0.035f, 0.035f, 0.01f, 0.02f, 0.02f, 0.03f, 1);
        if (fmodf(t, 1.4f) < 0.7f) bp_emissif(0.04f, 0.035f, -0.112f, 0.008f, 0.008f, 0.004f, 1, 0.05f, 0.05f);
        break;
    }
    case MB_CASSETTE:
        if (m->etat) break;                               /* deja prise */
        bp(0, 0.025f, 0, 0.1f, 0.025f, 0.06f, 0.05f, 0.05f, 0.05f, l);
        bp(0, 0.051f, 0, 0.07f, 0.002f, 0.035f, 0.85f, 0.85f, 0.8f, l);
        break;
    default: break;
    }
    glPopMatrix();
}

static void dessiner_meubles(float t) {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    fog_scene();
    for (int i = 0; i < nmeubles; i++) {
        const Meuble *m = &meubles[i];
        if (fabsf(m->x - camEye.x) > 25 || fabsf(m->z - camEye.z) > 25) continue;
        if (P.cache == 2 && i == P.armoire && appState == APP_GAME) continue;   /* on est dedans : la fente suffit */
        dessiner_meuble(m, t);
    }
    glColor3f(1, 1, 1);
}

/* avant l'image : la vue d'une camera (une par image, chacune son tour), si l'on est pres de la salle des cameras.
   Rendue en petit dans le tampon arriere, recopiee dans sa texture ; render() efface ensuite l'ecran. */
static void cctv_maj(float t, float dt) {
    if (!glReady || ncameras == 0 || appState != APP_GAME || cartonActif) return;
    int sc = -1;
    for (int i = 0; i < npieces; i++)
        if (pieces[i].zone == ZONE_CAMERAS && dist_xz(P.pos, v3(pieces[i].x0 + pieces[i].iw * 0.5f, 0, pieces[i].z0 + pieces[i].ih * 0.5f)) < 14) sc = i;
    if (sc < 0) { camSalleT = 0; return; }
    if (!texCam[0]) {
        glGenTextures(MAXCAMERAS, texCam);
        for (int i = 0; i < MAXCAMERAS; i++) {
            glBindTexture(GL_TEXTURE_2D, texCam[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, CAMTEX, CAMTEX, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        }
    }
    if (piece_ou(P.pos.x, P.pos.z) == sc) camSalleT += dt;
    int i = camTour++ % ncameras;
    CameraSurv cam = cameras[i];
    if (i == 0 && camSalleT > 3 && camSalleT < 9) {       /* l'ecran 1 montre... quelqu'un, de dos : toi */
        V3 f = view_dir(P.yaw, 0);
        float recul = fminf(2.4f, ray_grille(P.pos.x, P.pos.z, -f.x, -f.z, 2.4f) - 0.2f);
        cam.pos = v3(P.pos.x - f.x * recul, 2.35f, P.pos.z - f.z * recul); cam.yaw = P.yaw; cam.pitch = -0.35f;
    }
    int w0 = winW, h0 = winH; V3 e0 = camEye; float l0 = lampeK;
    winW = winH = CAMTEX;
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    set_camera(cam.pos, cam.yaw, cam.pitch, 0, 78);
    camEye = cam.pos; lampeK = 0;
    glEnable(GL_DEPTH_TEST);
    monde_draw(t, 0, 0, 0);
    dessiner_entites(t);
    dessiner_meubles(t);
    if (!P.cache && !P.dead) {                            /* le joueur local aussi, vu par la camera */
        int s = slot_local();
        Joueur sauve = JP[s];
        netj_de_P(&JP[s].st); JP[s].disp = P.pos; JP[s].actif = 1;
        dessiner_joueur(s, t);
        JP[s] = sauve;
    }
    glBindTexture(GL_TEXTURE_2D, texCam[i]);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, CAMTEX, CAMTEX);
    winW = w0; winH = h0; camEye = e0; lampeK = l0;
}
