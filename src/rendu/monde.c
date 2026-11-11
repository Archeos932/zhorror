/* BOURDONNEMENT - dessin du monde : texture x lightmap (multitexture, repris de VELOCITY), neons fous ajoutes,
   lampe torche projetee, panneaux des neons.
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).

   Passe 1 : unite 0 = texture du materiau, unite 1 = lightmap fixe (COMBINE MODULATE, echelle 2).
   Passe 2 (additive) : pour chaque neon fou / ascenseur ALLUME, ses quads avec la lightmap dynamique.
   Passe 3 (additive) : la lampe. Unite 1 = image du faisceau projetee depuis l'oeil (glTexGen EYE_LINEAR + matrice
   de texture en perspective) ; le brouillard noir de cette passe fait la portee. Rien n'est eclaire derriere un mur :
   seuls les pixels deja visibles (test de profondeur LEQUAL) recoivent la lampe.
   Sans multitexture : couleurs par sommet tirees de la lightmap, pas de lampe projetee. */
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif
#ifndef GL_MAX_TEXTURE_UNITS
#define GL_MAX_TEXTURE_UNITS 0x84E2
#endif
#ifndef GL_COMBINE
#define GL_COMBINE 0x8570
#endif
#ifndef GL_COMBINE_RGB
#define GL_COMBINE_RGB 0x8571
#endif
#ifndef GL_RGB_SCALE
#define GL_RGB_SCALE 0x8573
#endif
#ifndef GL_PREVIOUS
#define GL_PREVIOUS 0x8578
#endif
#ifndef GL_SOURCE0_RGB
#define GL_SOURCE0_RGB 0x8580
#endif
#ifndef GL_SOURCE1_RGB
#define GL_SOURCE1_RGB 0x8581
#endif
#ifndef GL_OPERAND0_RGB
#define GL_OPERAND0_RGB 0x8590
#endif
#ifndef GL_OPERAND1_RGB
#define GL_OPERAND1_RGB 0x8591
#endif
typedef void(WINAPI *MtActiveFn)(GLenum);
typedef void(WINAPI *MtCoordFn)(GLenum, GLfloat, GLfloat);
static MtActiveFn mtActive;
static MtCoordFn mtCoord;
static int mtUnits;                                       /* unites de texture utilisables (0 : pas de multitexture) */
static int glReady;                                       /* contexte OpenGL cree (--test n'en a pas) */
static GLuint lmTexFixe, lmTexDyn, listeMonde;
static GLuint listeDyn[MAXNEON];

static void world_gl_caps(void) {
    static int done;
    if (done) return;
    done = 1;
    mtUnits = 0;
    const char *e = (const char *)glGetString(GL_EXTENSIONS), *v = (const char *)glGetString(GL_VERSION);
    int major = v ? atoi(v) : 1, minor = v && strchr(v, '.') ? atoi(strchr(v, '.') + 1) : 0;
    int gl13 = major > 1 || (major == 1 && minor >= 3);
    if (!gl13 && !(e && strstr(e, "GL_ARB_multitexture") && strstr(e, "GL_ARB_texture_env_combine"))) return;
#ifdef _WIN32
    mtActive = (MtActiveFn)wglGetProcAddress("glActiveTextureARB");
    mtCoord = (MtCoordFn)wglGetProcAddress("glMultiTexCoord2fARB");
#else
    mtActive = glActiveTexture;
    mtCoord = glMultiTexCoord2f;
#endif
    if (!mtActive || !mtCoord) return;
    GLint u = 1;
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &u);
    mtUnits = u >= 2 ? u : 0;
}

static void face_tex_uv(const Face *f, V3 p, float *u, float *v) {
    float e = matEchelle[f->mat];
    if (fabsf(f->n.y) > 0.5f) { *u = p.x * e; *v = p.z * e; }
    else if (fabsf(f->n.x) > 0.5f) { *u = p.z * e; *v = -p.y * e; }
    else { *u = p.x * e; *v = -p.y * e; }
}

static void face_emit(const Face *f, const unsigned char *atlasCpu) {
    static const float cu[4] = { 0, 1, 1, 0 }, cv[4] = { 0, 0, 1, 1 };
    float inv = 1.0f / lmTaille;
    for (int k = 0; k < 4; k++) {
        V3 p = vadd(f->o, vadd(vmul(f->u, cu[k]), vmul(f->v, cv[k])));
        float u, v, lu = (f->lx + cu[k] * f->lw) * inv, lv = (f->ly + cv[k] * f->lh) * inv;
        face_tex_uv(f, p, &u, &v);
        if (mtUnits) { mtCoord(GL_TEXTURE0, u, v); mtCoord(GL_TEXTURE1, lu, lv); }
        else {                                            /* repli : la lightmap au coin, en couleur de sommet */
            int tx = f->lx + (cu[k] > 0 ? f->lw - 1 : 0), ty = f->ly + (cv[k] > 0 ? f->lh - 1 : 0);
            const unsigned char *q = atlasCpu + ((size_t)ty * lmTaille + tx) * 3;
            glColor3f(q[0] / 127.5f, q[1] / 127.5f, q[2] / 127.5f);
            glTexCoord2f(u, v);
        }
        glVertex3f(p.x, p.y, p.z);
    }
}

/* une liste : les faces retenues (dyn == quel ; -2 = toutes), groupees par materiau */
static void liste_faces(int quel, const unsigned char *atlasCpu) {
    for (int m = 0; m < MAT_COUNT; m++) {
        int vide = 1;
        for (int i = 0; i < nfaces; i++) if (faces[i].mat == m && (quel == -2 || faces[i].dyn == quel)) { vide = 0; break; }
        if (vide) continue;
        glBindTexture(GL_TEXTURE_2D, texMat[m]);
        glBegin(GL_QUADS);
        for (int i = 0; i < nfaces; i++) if (faces[i].mat == m && (quel == -2 || faces[i].dyn == quel)) face_emit(&faces[i], atlasCpu);
        glEnd();
    }
}

static void upload_lm(GLuint *t, const unsigned char *data) {
    if (!*t) glGenTextures(1, t);
    glBindTexture(GL_TEXTURE_2D, *t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, lmTaille, lmTaille, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
}

/* nouveau niveau : precalcul (toujours), puis textures et listes (s'il y a un contexte OpenGL) */
static void monde_reconstruire(void) {
    bake_lightmaps();
    if (!glReady) return;
    world_gl_caps();
    upload_lm(&lmTexFixe, lmFixe);
    upload_lm(&lmTexDyn, lmDyn);
    if (listeMonde) glDeleteLists(listeMonde, 1);
    listeMonde = glGenLists(1);
    if (mtUnits) mtActive(GL_TEXTURE0);
    glNewList(listeMonde, GL_COMPILE); liste_faces(-2, lmFixe); glEndList();
    for (int i = 0; i < MAXNEON; i++) { if (listeDyn[i]) glDeleteLists(listeDyn[i], 1); listeDyn[i] = 0; }
    if (mtUnits)
        for (int i = 0; i < nneons; i++) {
            if (!neon_dynamique(i)) continue;
            listeDyn[i] = glGenLists(1);
            glNewList(listeDyn[i], GL_COMPILE); liste_faces(i, lmDyn); glEndList();
        }
}

static void unite1_lightmap(GLuint tex) {
    mtActive(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
    mtActive(GL_TEXTURE0);
}
static void unite1_off(void) {
    mtActive(GL_TEXTURE1);
    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 1.0f);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_GEN_S); glDisable(GL_TEXTURE_GEN_T); glDisable(GL_TEXTURE_GEN_R); glDisable(GL_TEXTURE_GEN_Q);
    glMatrixMode(GL_TEXTURE); glLoadIdentity(); glMatrixMode(GL_MODELVIEW);
    glDisable(GL_TEXTURE_2D);
    mtActive(GL_TEXTURE0);
}

/* fog : couleur et densite du niveau (exp2) */
static void fog_scene(void) {
    GLfloat fc[4] = { 0.012f, 0.010f, 0.005f, 1 };
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_EXP2);
    glFogf(GL_FOG_DENSITY, niveauFog);
    glFogfv(GL_FOG_COLOR, fc);
}

/* lampe : intensite k (0..1), direction du faisceau relative au regard (dyaw, dpitch en radians) */
static void monde_draw(float t, float lampe, float dyaw, float dpitch) {
    GLfloat noir[4] = { 0, 0, 0, 1 };
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glColor3f(1, 1, 1);
    fog_scene();
    if (mtUnits) unite1_lightmap(lmTexFixe);
    glCallList(listeMonde);

    if (mtUnits) {
        glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE);
        glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE);
        glFogfv(GL_FOG_COLOR, noir);
        unite1_lightmap(lmTexDyn);                        /* neons fous et ascenseur allumes */
        for (int i = 0; i < nneons; i++) if (listeDyn[i] && neon_allume(i, t)) glCallList(listeDyn[i]);

        if (lampe > 0.01f) {                              /* la lampe */
            mtActive(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, texLampe);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
            glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
            GLfloat ps[4] = { 1, 0, 0, 0 }, pt[4] = { 0, 1, 0, 0 }, pr[4] = { 0, 0, 1, 0 }, pq[4] = { 0, 0, 0, 1 };
            glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();   /* plans en coordonnees de l'oeil */
            glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR); glTexGenfv(GL_S, GL_EYE_PLANE, ps);
            glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR); glTexGenfv(GL_T, GL_EYE_PLANE, pt);
            glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR); glTexGenfv(GL_R, GL_EYE_PLANE, pr);   /* z : la projection en a besoin */
            glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR); glTexGenfv(GL_Q, GL_EYE_PLANE, pq);
            glPopMatrix();
            glEnable(GL_TEXTURE_GEN_S); glEnable(GL_TEXTURE_GEN_T); glEnable(GL_TEXTURE_GEN_R); glEnable(GL_TEXTURE_GEN_Q);
            glMatrixMode(GL_TEXTURE); glLoadIdentity();
            glTranslatef(0.5f, 0.5f, 0); glScalef(0.5f, 0.5f, 1);
            float a = tanf(LAMPE_ANGLE * PI / 180.0f);
            glFrustum(-a, a, -a, a, 1, 100);
            glRotatef(-dpitch * 180.0f / PI, 1, 0, 0);     /* le faisceau traine un peu derriere le regard */
            glRotatef(dyaw * 180.0f / PI, 0, 1, 0);
            glTranslatef(-0.12f, 0.18f, 0);                /* lampe tenue a droite, un peu plus bas que l'oeil */
            glMatrixMode(GL_MODELVIEW);
            mtActive(GL_TEXTURE0);
            glFogi(GL_FOG_MODE, GL_EXP);
            glFogf(GL_FOG_DENSITY, LAMPE_PORTEE_FOG);
            glColor3f(lampe * 0.85f, lampe * 0.85f, lampe * 0.85f);
            glCallList(listeMonde);
            glColor3f(1, 1, 1);
        }
        for (int k = 0; k < MAXP; k++) {                  /* les lampes des autres joueurs eclairent aussi */
            Joueur *j = &JP[k];
            if (netMode == NET_OFF || k == monSlot || !j->actif || !(j->st.flags & JF_LAMPE) || (j->st.flags & JF_MORT)) continue;
            V3 lp = v3(j->disp.x, j->st.oeil / 100.0f - 0.15f, j->disp.z), f = view_dir(j->st.lampeYaw, j->st.lampePitch);
            if (vlen(vsub(lp, camEye)) > 30) continue;
            GLdouble eq[4] = { f.x, f.y, f.z, -(f.x * lp.x + f.y * lp.y + f.z * lp.z) - 0.2 };   /* rien derriere sa lampe */
            glClipPlane(GL_CLIP_PLANE0, eq);
            glEnable(GL_CLIP_PLANE0);
            mtActive(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, texLampe);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
            glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
            GLfloat ps[4] = { 1, 0, 0, 0 }, pt[4] = { 0, 1, 0, 0 }, pr[4] = { 0, 0, 1, 0 }, pq[4] = { 0, 0, 0, 1 };
            glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
            glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR); glTexGenfv(GL_S, GL_EYE_PLANE, ps);
            glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR); glTexGenfv(GL_T, GL_EYE_PLANE, pt);
            glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR); glTexGenfv(GL_R, GL_EYE_PLANE, pr);
            glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR); glTexGenfv(GL_Q, GL_EYE_PLANE, pq);
            glPopMatrix();
            glEnable(GL_TEXTURE_GEN_S); glEnable(GL_TEXTURE_GEN_T); glEnable(GL_TEXTURE_GEN_R); glEnable(GL_TEXTURE_GEN_Q);
            glMatrixMode(GL_TEXTURE); glLoadIdentity();   /* oeil local -> monde -> lampe de l'autre -> image */
            glTranslatef(0.5f, 0.5f, 0); glScalef(0.5f, 0.5f, 1);
            float a = tanf(LAMPE_ANGLE * PI / 180.0f);
            glFrustum(-a, a, -a, a, 1, 100);
            glRotatef(-j->st.lampePitch * 180.0f / PI, 1, 0, 0);
            glRotatef(j->st.lampeYaw * 180.0f / PI, 0, 1, 0);
            glTranslatef(-lp.x, -lp.y, -lp.z);
            glTranslatef(camEye.x, camEye.y, camEye.z);
            glRotatef(-camYaw * 180.0f / PI, 0, 1, 0);
            glRotatef(camPitch * 180.0f / PI, 1, 0, 0);
            glRotatef(-camRoll, 0, 0, 1);
            glMatrixMode(GL_MODELVIEW);
            mtActive(GL_TEXTURE0);
            glFogi(GL_FOG_MODE, GL_EXP);
            glFogf(GL_FOG_DENSITY, LAMPE_PORTEE_FOG);
            glColor3f(0.7f, 0.7f, 0.7f);
            glCallList(listeMonde);
            glColor3f(1, 1, 1);
            glDisable(GL_CLIP_PLANE0);
        }
        unite1_off();
        glDepthMask(GL_TRUE); glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);
        fog_scene();
    }

    /* panneaux des neons (emissifs) */
    glBindTexture(GL_TEXTURE_2D, texNeon);
    glBegin(GL_QUADS);
    for (int i = 0; i < nneons; i++) {
        float on = neon_allume(i, t) ? 1.0f : 0.07f, x = neons[i].x, z = neons[i].z, y = HAUT_PLAFOND - 0.004f;
        glColor3f(on, on, on * 0.97f);
        glTexCoord2f(0, 0); glVertex3f(x - 0.6f, y, z - 0.3f);   /* vu d'en dessous : sens direct */
        glTexCoord2f(1, 0); glVertex3f(x + 0.6f, y, z - 0.3f);
        glTexCoord2f(1, 1); glVertex3f(x + 0.6f, y, z + 0.3f);
        glTexCoord2f(0, 1); glVertex3f(x - 0.6f, y, z + 0.3f);
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glColor3f(1, 1, 1);
}
