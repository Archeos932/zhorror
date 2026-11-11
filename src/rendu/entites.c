/* BOURDONNEMENT - ce qui bouge ou se ramasse : le Rodeur, les fusibles, l'ascenseur ; et la tete du screamer
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   Pas d'eclairage OpenGL : chaque piece recoit une couleur = base x (lumiere des neons de sa case + lampe si elle
   est dans le faisceau), avec un ombrage par face. La peur vient de la silhouette, pas du detail. */

static V3 camEye;                                         /* oeil de l'image en cours (render) */
static float lampeK;                                      /* intensite de la lampe pour cette image */

/* lumiere recue en p : neons (fixes + dynamiques proches) + lampe */
static float lum_entite(V3 p, float t) {
    float l = lum_case(p.x, p.z) * 0.9f;
    for (int i = 0; i < nneons; i++) {
        if (!neon_dynamique(i) || !neon_allume(i, t)) continue;
        float dx = neons[i].x - p.x, dz = neons[i].z - p.z, d2 = dx * dx + dz * dz;
        if (d2 < 25 && visible_xz(p.x, p.z, neons[i].x, neons[i].z)) l += 0.9f / (1 + d2 * 0.5f);
    }
    if (lampeK > 0.01f) {
        V3 dl = view_dir(P.lampeYaw, P.lampePitch), to = vsub(p, camEye);
        float d = vlen(to);
        if (d > 0.01f) {
            float c = vdot(dl, to) / d, ca = cosf(LAMPE_ANGLE * PI / 180.0f);
            if (c > ca && visible_xz(camEye.x, camEye.z, p.x, p.z)) {
                float k = clampf((c - ca) / (1 - ca), 0, 1);
                l += lampeK * 1.5f * (k * k * 0.8f + 0.2f) * expf(-d * LAMPE_PORTEE_FOG);
            }
        }
    }
    return l;
}

/* boite centree (0, 0, 0) de demi-tailles h, dans le repere courant ; c = couleur deja eclairee */
static void boite(float hx, float hy, float hz, float r, float g, float b) {
    static const float sh[6] = { 0.8f, 0.8f, 1.0f, 0.45f, 0.9f, 0.7f };   /* +x -x +y -y +z -z */
    glBegin(GL_QUADS);
    glColor3f(r * sh[0], g * sh[0], b * sh[0]);
    glVertex3f(hx, -hy, -hz); glVertex3f(hx, hy, -hz); glVertex3f(hx, hy, hz); glVertex3f(hx, -hy, hz);
    glColor3f(r * sh[1], g * sh[1], b * sh[1]);
    glVertex3f(-hx, -hy, hz); glVertex3f(-hx, hy, hz); glVertex3f(-hx, hy, -hz); glVertex3f(-hx, -hy, -hz);
    glColor3f(r * sh[2], g * sh[2], b * sh[2]);
    glVertex3f(-hx, hy, -hz); glVertex3f(-hx, hy, hz); glVertex3f(hx, hy, hz); glVertex3f(hx, hy, -hz);
    glColor3f(r * sh[3], g * sh[3], b * sh[3]);
    glVertex3f(-hx, -hy, hz); glVertex3f(-hx, -hy, -hz); glVertex3f(hx, -hy, -hz); glVertex3f(hx, -hy, hz);
    glColor3f(r * sh[4], g * sh[4], b * sh[4]);
    glVertex3f(-hx, -hy, hz); glVertex3f(hx, -hy, hz); glVertex3f(hx, hy, hz); glVertex3f(-hx, hy, hz);
    glColor3f(r * sh[5], g * sh[5], b * sh[5]);
    glVertex3f(hx, -hy, -hz); glVertex3f(-hx, -hy, -hz); glVertex3f(-hx, hy, -hz); glVertex3f(hx, hy, -hz);
    glEnd();
}

/* membre qui pend d'une articulation : longueur L vers le bas, epaisseur e */
static void membre(float L, float e, float r, float g, float b) {
    glPushMatrix(); glTranslatef(0, -L * 0.5f, 0); boite(e, L * 0.5f, e, r, g, b); glPopMatrix();
}

/* la tete du Rodeur, centree a l'origine, face vers -z : longue, sans yeux (deux orbites noires), bouche qui
   s'ouvre (bouche 0..1) : la machoire se decroche, beaucoup trop bas */
static void tete(float l, float bouche) {
    float pr = 0.52f * l, pg = 0.49f * l, pb = 0.45f * l;
    glPushMatrix(); glTranslatef(0, 0.04f, 0); boite(0.1f, 0.16f, 0.11f, pr, pg, pb); glPopMatrix();   /* crane */
    for (int s = -1; s <= 1; s += 2) {                    /* orbites vides */
        glPushMatrix(); glTranslatef(s * 0.045f, 0.07f, -0.108f); boite(0.03f, 0.024f, 0.006f, 0.01f, 0.005f, 0.005f); glPopMatrix();
    }
    float ouv = 0.03f + 0.11f * bouche;                   /* hauteur de la bouche */
    glPushMatrix(); glTranslatef(0, -0.07f - ouv * 0.5f, -0.106f);
    boite(0.045f + 0.025f * bouche, ouv * 0.5f + 0.01f, 0.006f, 0.03f * l, 0.0f, 0.0f);   /* le trou, noir-rouge */
    glPopMatrix();
    for (int d = -3; d <= 3; d++) {                       /* dents du haut */
        glPushMatrix(); glTranslatef(d * 0.013f, -0.075f, -0.112f);
        boite(0.0045f, 0.014f + 0.006f * (d & 1), 0.004f, 0.78f * l, 0.74f * l, 0.6f * l); glPopMatrix();
    }
    glPushMatrix(); glTranslatef(0, -0.1f - ouv, -0.01f);  /* machoire */
    glRotatef(-10 * bouche, 1, 0, 0);
    boite(0.08f, 0.035f, 0.1f, pr * 0.9f, pg * 0.9f, pb * 0.9f);
    for (int d = -3; d <= 3; d++) {
        glPushMatrix(); glTranslatef(d * 0.013f, 0.04f, -0.095f);
        boite(0.0045f, 0.013f + 0.007f * ((d + 1) & 1), 0.004f, 0.75f * l, 0.71f * l, 0.58f * l); glPopMatrix();
    }
    glPopMatrix();
}

/* le Rodeur, a l'origine du repere courant, regardant vers -z. anim : phase de marche ; l : lumiere ;
   bouche : 0 fermee .. 1 grande ouverte (screamer) ; tic : spasmes de la tete */
static void dessiner_rodeur(float anim, float l, float bouche, float tic) {
    float pr = 0.50f * l, pg = 0.47f * l, pb = 0.43f * l;   /* peau grise, presque invisible dans le noir */
    float q = floorf(anim * 5) / 5;                       /* pose qui change par a-coups */
    float sw = sinf(q * 3.0f) * 26, sa = -sw * 0.6f;
    for (int s = -1; s <= 1; s += 2) {                    /* jambes : tres longues, genoux a l'envers */
        glPushMatrix(); glTranslatef(s * 0.12f, 1.28f, 0); glRotatef(s * sw, 1, 0, 0);
        membre(0.66f, 0.055f, pr, pg, pb);
        glTranslatef(0, -0.66f, 0); glRotatef(-18 - fabsf(sw) * 0.4f, 1, 0, 0);
        membre(0.64f, 0.045f, pr * 0.9f, pg * 0.9f, pb * 0.9f);
        glPopMatrix();
    }
    glPushMatrix(); glTranslatef(0, 1.28f, 0); glRotatef(-14, 1, 0, 0);   /* torse voute */
    glTranslatef(0, 0.42f, 0); boite(0.15f, 0.42f, 0.085f, pr * 0.95f, pg * 0.95f, pb * 0.95f);
    for (int k = 0; k < 4; k++) {                         /* cotes saillantes */
        glPushMatrix(); glTranslatef(0, 0.1f - k * 0.12f, -0.09f); boite(0.13f, 0.012f, 0.01f, pr * 0.7f, pg * 0.68f, pb * 0.65f); glPopMatrix();
    }
    for (int s = -1; s <= 1; s += 2) {                    /* bras : jusqu'aux genoux, doigts trop longs */
        glPushMatrix(); glTranslatef(s * 0.22f, 0.36f, 0); glRotatef(s * 7.0f, 0, 0, 1); glRotatef(s * sa + 8, 1, 0, 0);
        membre(0.72f, 0.04f, pr, pg, pb);
        glTranslatef(0, -0.72f, 0); glRotatef(-25, 1, 0, 0);
        membre(0.62f, 0.035f, pr * 0.92f, pg * 0.92f, pb * 0.92f);
        glTranslatef(0, -0.62f, 0);
        for (int d = -1; d <= 1; d++) { glPushMatrix(); glTranslatef(d * 0.025f, 0, 0); glRotatef(d * 8.0f - 10, 1, 0, 0); membre(0.22f, 0.01f, pr * 0.8f, pg * 0.8f, pb * 0.8f); glPopMatrix(); }
        glPopMatrix();
    }
    glTranslatef(0, 0.42f, 0);                            /* cou, puis la tete : penchee, sans yeux */
    glRotatef(tic, 0, 1, 0);
    glPushMatrix(); glTranslatef(0, 0.09f, -0.02f); boite(0.035f, 0.09f, 0.035f, pr, pg, pb); glPopMatrix();
    glTranslatef(0, 0.3f, -0.04f); glRotatef(38 + tic * 0.3f, 0, 0, 1);
    tete(l, bouche);
    glPopMatrix();
}

static void dessiner_entites(float t) {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    fog_scene();
    /* le Rodeur */
    if (R.actif) {
        V3 p = vlerp(R.prevPos, R.pos, 1);
        float l = lum_entite(v3(p.x, 1.6f, p.z), t);
        float tic = R.etat == R_ECOUTE ? sinf(t * 37) * 18 * (sinf(t * 3) > 0.3f) : 0;
        glPushMatrix(); glTranslatef(p.x, 0, p.z); glRotatef(-R.yaw * 180.0f / PI, 0, 1, 0);
        dessiner_rodeur(R.anim, l, 0, tic);
        glPopMatrix();
    }
    /* apparition : la meme chose, face a toi, immobile */
    if (apparitionT >= 0) {
        V3 p = apparitionPos;
        float yaw = atan2f(P.pos.x - p.x, -(P.pos.z - p.z));
        float l = lum_entite(v3(p.x, 1.6f, p.z), t) + 0.6f;
        glPushMatrix(); glTranslatef(p.x, 0, p.z); glRotatef(-yaw * 180.0f / PI, 0, 1, 0);
        dessiner_rodeur(0, l, 0.3f, sinf(t * 50) * 10);
        glPopMatrix();
    }
    /* fusibles : petite boite grise et voyant rouge qui clignote (repere dans le noir) */
    for (int i = 0; i < nfus; i++) {
        if (fus[i].pris) continue;
        V3 p = fus[i].pos;
        float l = lum_entite(v3(p.x, 0.3f, p.z), t);
        glPushMatrix(); glTranslatef(p.x, 0.07f, p.z); glRotatef((float)(i * 37), 0, 1, 0);
        boite(0.14f, 0.07f, 0.09f, 0.45f * l, 0.45f * l, 0.48f * l);
        glTranslatef(0, 0.07f, -0.03f); boite(0.05f, 0.012f, 0.025f, 0.55f * l, 0.15f * l, 0.1f * l);   /* la cartouche */
        float led = fmodf(t + i * 0.37f, 1.6f) < 0.12f ? 1.0f : 0.15f;
        glTranslatef(0.09f, 0, 0.04f); glDisable(GL_FOG); boite(0.012f, 0.012f, 0.012f, led, led * 0.08f, led * 0.05f); glEnable(GL_FOG);
        glPopMatrix();
    }
    /* ascenseur : cadre, voyant, portes coulissantes */
    if (asc.actif) {
        int k = asc.cote;
        float nx = (float)cotes[k][0], nz = (float)cotes[k][1];
        V3 c = v3(asc.gx + 0.5f + nx * 0.5f, 0, asc.gz + 0.5f + nz * 0.5f);   /* milieu du seuil */
        V3 u = v3(-nz, 0, nx);                            /* le long du seuil */
        float l = lum_entite(v3(c.x + nx * 0.5f, 1.2f, c.z + nz * 0.5f), t) + (asc.alimente ? 0.3f : 0);
        glBindTexture(GL_TEXTURE_2D, texMetal);
        glEnable(GL_TEXTURE_2D);
        float o = asc.ouverture, hp = 0.5f * (1 - o);     /* largeur de chaque porte */
        for (int s = -1; s <= 1; s += 2) {
            V3 a = vadd(c, vmul(u, s * 0.5f)), b = vadd(c, vmul(u, s * (0.5f - hp)));
            glColor3f(0.85f * l, 0.86f * l, 0.9f * l);
            glBegin(GL_QUADS);
            glTexCoord2f(0, 0); glVertex3f(a.x, 0, a.z); glTexCoord2f(hp, 0); glVertex3f(b.x, 0, b.z);
            glTexCoord2f(hp, 2.2f); glVertex3f(b.x, 2.2f, b.z); glTexCoord2f(0, 2.2f); glVertex3f(a.x, 2.2f, a.z);
            glEnd();
        }
        glColor3f(0.6f * l, 0.6f * l, 0.62f * l);         /* linteau au-dessus des portes */
        glBegin(GL_QUADS);
        V3 a = vadd(c, vmul(u, -0.5f)), b = vadd(c, vmul(u, 0.5f));
        glTexCoord2f(0, 0); glVertex3f(a.x, 2.2f, a.z); glTexCoord2f(1, 0); glVertex3f(b.x, 2.2f, b.z);
        glTexCoord2f(1, 0.6f); glVertex3f(b.x, HAUT_PLAFOND, b.z); glTexCoord2f(0, 0.6f); glVertex3f(a.x, HAUT_PLAFOND, a.z);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_FOG);                                /* voyant : rouge sans courant, vert avec (visible de loin) */
        glPushMatrix(); glTranslatef(c.x + nx * 0.02f, 2.45f, c.z + nz * 0.02f);
        float bl = asc.alimente ? 1.0f : (fmodf(t, 2.0f) < 1.0f ? 0.8f : 0.3f);
        if (asc.alimente) boite(0.04f, 0.04f, 0.04f, 0.1f, bl, 0.2f); else boite(0.04f, 0.04f, 0.04f, bl, 0.05f, 0.03f);
        glPopMatrix();
        glEnable(GL_FOG);
    }
    glColor3f(1, 1, 1);
}

/* screamer : la tete, collee a la camera (appele avec une matrice de vue identite) */
static void dessiner_screamer(float s) {
    float shake = (1 - fminf(1, s / SCREAMER_MORT_T)) * 0.03f;
    float flash = fmodf(s, 0.12f) < 0.06f ? 1.7f : 0.85f;
    float z = -1.1f + fminf(s / 0.12f, 1) * 0.55f;       /* elle se jette sur toi en 0,12 s */
    glDisable(GL_FOG); glDisable(GL_TEXTURE_2D);
    glPushMatrix();
    glTranslatef(sinf(s * 91) * shake, 0.03f + cosf(s * 77) * shake, z);
    glRotatef(180, 0, 1, 0);                              /* face a toi */
    glRotatef(sinf(s * 40) * 8 + 12, 0, 0, 1);
    tete(flash, fminf(1, s * 6));
    glPopMatrix();
}
