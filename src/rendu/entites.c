/* BOURDONNEMENT - ce qui bouge ou se ramasse : le Rodeur, les fusibles, l'ascenseur ; et la tete du screamer
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   Pas d'eclairage OpenGL : chaque piece recoit une couleur = base x (lumiere des neons de sa case + lampe si elle
   est dans le faisceau), avec un ombrage par face. La peur vient de la silhouette, pas du detail. */

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

/* tete du Locataire : visage d'homme trop pale, yeux noirs, sourire trop large */
static void tete_locataire(float l, float bouche) {
    float pr = 0.70f * l, pg = 0.64f * l, pb = 0.56f * l;
    boite(0.085f, 0.115f, 0.1f, pr, pg, pb);
    glPushMatrix(); glTranslatef(0, 0.1f, 0.01f); boite(0.09f, 0.03f, 0.105f, 0.1f * l, 0.08f * l, 0.06f * l); glPopMatrix();   /* cheveux */
    for (int s = -1; s <= 1; s += 2) {                    /* yeux tout noirs */
        glPushMatrix(); glTranslatef(s * 0.035f, 0.025f, -0.1f); boite(0.018f, 0.013f, 0.004f, 0, 0, 0); glPopMatrix();
    }
    glPushMatrix(); glTranslatef(0, -0.055f - 0.03f * bouche, -0.1f);   /* le sourire, d'une oreille a l'autre */
    boite(0.075f, 0.006f + 0.035f * bouche, 0.004f, 0.02f, 0, 0);
    for (int d = -5; d <= 5; d++) { glPushMatrix(); glTranslatef(d * 0.013f, 0.004f + 0.03f * bouche, -0.003f); boite(0.005f, 0.006f, 0.003f, 0.8f * l, 0.78f * l, 0.66f * l); glPopMatrix(); }
    glPopMatrix();
}

/* le Locataire : homme trop grand en chemise de bureau jaunie, tete couchee sur l'epaule, bras le long du corps */
static void dessiner_locataire(float anim, float l, int fige) {
    float q = fige ? floorf(anim * 4) / 4 : anim;
    float sw = sinf(q * 3.0f) * 16;
    float cr = 0.58f * l, cg = 0.52f * l, cb = 0.30f * l;   /* chemise */
    for (int s = -1; s <= 1; s += 2) {                    /* jambes, pantalon sombre */
        glPushMatrix(); glTranslatef(s * 0.1f, 1.05f, 0); glRotatef(s * sw, 1, 0, 0);
        membre(1.05f, 0.065f, 0.11f * l, 0.1f * l, 0.09f * l);
        glPopMatrix();
    }
    glPushMatrix(); glTranslatef(0, 1.45f, 0); boite(0.2f, 0.4f, 0.11f, cr, cg, cb);   /* torse */
    glPushMatrix(); glTranslatef(0, 0.2f, -0.112f); boite(0.025f, 0.2f, 0.004f, 0.25f * l, 0.05f * l, 0.05f * l); glPopMatrix();   /* cravate */
    for (int s = -1; s <= 1; s += 2) {                    /* bras raides, mains trop longues */
        glPushMatrix(); glTranslatef(s * 0.25f, 0.36f, 0); glRotatef(-s * sw * 0.3f, 1, 0, 0);
        membre(0.95f, 0.05f, cr * 0.95f, cg * 0.95f, cb * 0.95f);
        glTranslatef(0, -0.95f, 0); membre(0.25f, 0.04f, 0.65f * l, 0.6f * l, 0.52f * l);
        glPopMatrix();
    }
    glTranslatef(0, 0.45f, 0); boite(0.04f, 0.06f, 0.04f, 0.65f * l, 0.6f * l, 0.52f * l);   /* cou */
    glTranslatef(0.13f, -0.02f, 0); glRotatef(-82, 0, 0, 1);   /* la tete, couchee a angle droit */
    tete_locataire(l, 0.25f);
    glPopMatrix();
}

/* tete de la Rampante : enormes yeux (qui brillent dans le noir), gueule fendue */
static void tete_rampante(float l, float bouche, float eclat) {
    float pr = 0.66f * l, pg = 0.64f * l, pb = 0.6f * l;
    boite(0.13f, 0.09f, 0.12f, pr, pg, pb);
    glDisable(GL_FOG);
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix(); glTranslatef(s * 0.06f, 0.025f, -0.118f);
        boite(0.042f, 0.035f, 0.006f, eclat, eclat, eclat * 0.85f);
        glTranslatef(0, 0, -0.004f); boite(0.008f, 0.03f, 0.003f, 0, 0, 0);   /* pupille fendue */
        glPopMatrix();
    }
    glEnable(GL_FOG);
    glPushMatrix(); glTranslatef(0, -0.06f, -0.118f); boite(0.1f, 0.012f + 0.06f * bouche, 0.004f, 0.03f, 0, 0);
    for (int d = -6; d <= 6; d++) { glPushMatrix(); glTranslatef(d * 0.015f, 0.008f + 0.05f * bouche, -0.003f); boite(0.004f, 0.012f, 0.003f, 0.85f * l, 0.83f * l, 0.72f * l); glPopMatrix(); }
    glPopMatrix();
}

/* la Rampante : a quatre pattes, longues pattes d'araignee, corps colle au sol */
static void dessiner_rampante(float anim, float l, float t, float eclat) {
    float pr = 0.62f * l, pg = 0.6f * l, pb = 0.56f * l;
    glPushMatrix(); glTranslatef(0, 0.42f, 0); boite(0.17f, 0.1f, 0.5f, pr, pg, pb);   /* corps */
    for (int k = 0; k < 6; k++) { glPushMatrix(); glTranslatef(0, 0.1f, -0.35f + k * 0.13f); boite(0.03f, 0.02f, 0.03f, pr * 0.8f, pg * 0.8f, pb * 0.8f); glPopMatrix(); }   /* vertebres */
    for (int s = -1; s <= 1; s += 2)
        for (int av = -1; av <= 1; av += 2) {             /* pattes : le genou plus haut que le dos */
            float ph = sinf(anim * 4 + (s * av > 0 ? 0 : PI)) * 25;
            glPushMatrix(); glTranslatef(s * 0.15f, 0.05f, av * -0.38f);
            glRotatef(-s * 55.0f, 0, 0, 1); glRotatef(ph, 1, 0, 0);   /* cuisse : vers le haut et l'exterieur */
            glTranslatef(0, 0.25f, 0); boite(0.03f, 0.25f, 0.03f, pr, pg, pb);
            glTranslatef(0, 0.25f, 0); glRotatef(s * 150.0f, 0, 0, 1);   /* tibia : redescend vers le sol, plus loin */
            glTranslatef(0, 0.36f, 0); boite(0.025f, 0.36f, 0.025f, pr * 0.9f, pg * 0.9f, pb * 0.9f);
            glPopMatrix();
        }
    glTranslatef(0, 0.02f + 0.03f * sinf(t * 31), -0.62f); glRotatef(sinf(t * 7) * 12, 0, 0, 1);
    tete_rampante(l, 0.3f + 0.2f * sinf(t * 3), eclat);
    glPopMatrix();
}

/* n'importe quel monstre, a l'origine, regardant vers -z */
static void dessiner_monstre(int type, float anim, float l, float t, int etat) {
    if (type == MON_LOCATAIRE) dessiner_locataire(anim, l, etat == E_FIGE);
    else if (type == MON_RAMPANTE) dessiner_rampante(anim, l, t, 0.35f + l * 0.6f);
    else dessiner_rodeur(anim, l, etat == E_CHASSE ? 0.35f : 0.1f, etat == E_ECOUTE ? sinf(t * 37) * 18 * (sinf(t * 3) > 0.3f) : 0);
}

/* sol : flaques (sombres, luisantes, rides) et verre brise (eclats qui accrochent la lampe) */
static void dessiner_sol(float t) {
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glBegin(GL_QUADS);
    for (int z = 0; z < carteH; z++)
        for (int x = 0; x < carteW; x++) {
            if (sol[z][x] != SOL_EAU) continue;
            for (int sz = 0; sz < 2; sz++)
                for (int sx = 0; sx < 2; sx++) {
                    static const float cu[4] = { 0, 0, 1, 1 }, cv[4] = { 0, 1, 1, 0 };
                    for (int k = 0; k < 4; k++) {
                        float px = x + (sx + cu[k]) * 0.5f, pz = z + (sz + cv[k]) * 0.5f;
                        float ride = 0.5f + 0.5f * sinf(t * 1.7f + px * 4.1f + pz * 3.3f) * sinf(t * 1.1f - px * 2.3f + pz * 5.2f);
                        float l = lum_entite(v3(px, 0.05f, pz), t);
                        float c = 0.03f + l * (0.12f + 0.35f * ride * ride);
                        glColor4f(c * 0.8f, c * 0.9f, c, 0.62f);
                        glVertex3f(px, 0.006f, pz);
                    }
                }
        }
    glEnd();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBegin(GL_TRIANGLES);
    for (int z = 0; z < carteH; z++)
        for (int x = 0; x < carteW; x++) {
            if (sol[z][x] != SOL_VERRE) continue;
            float l = lum_entite(v3(x + 0.5f, 0.05f, z + 0.5f), t);
            for (int k = 0; k < 14; k++) {
                unsigned h = hash32((unsigned)(x * 7919 + z * 104729 + k * 31));
                float px = x + (h & 255) / 255.0f, pz = z + ((h >> 8) & 255) / 255.0f, r = 0.03f + ((h >> 16) & 15) / 300.0f, a = (h >> 20) * 0.001f;
                float e = ((h >> 24) & 7) == 0 ? 2.2f : 1.0f;   /* certains eclats brillent */
                glColor3f(0.55f * l * e, 0.62f * l * e, 0.66f * l * e);
                glVertex3f(px + cosf(a) * r, 0.008f, pz + sinf(a) * r);
                glVertex3f(px + cosf(a + 2.2f) * r, 0.008f, pz + sinf(a + 2.2f) * r);
                glVertex3f(px + cosf(a + 4.0f) * r * 0.6f, 0.008f, pz + sinf(a + 4.0f) * r * 0.6f);
            }
        }
    glEnd();
}

/* machoire de loup : anneau de dents, ouvert (arme) ou referme */
static void dessiner_machoire(const Piege *pg, float t) {
    float l = lum_entite(v3(pg->pos.x, 0.1f, pg->pos.z), t) * 1.4f + 0.05f;   /* metal : il accroche la lumiere */
    glPushMatrix(); glTranslatef(pg->pos.x, 0.02f, pg->pos.z);
    int gx = (int)floorf(pg->pos.x), gz = (int)floorf(pg->pos.z);
    if (!cell_mur(gx - 1, gz) && !cell_mur(gx + 1, gz)) glRotatef(90, 0, 1, 0);   /* en travers du couloir */
    boite(0.07f, 0.02f, 0.07f, 0.45f * l, 0.42f * l, 0.4f * l);   /* plaque */
    glPushMatrix(); glTranslatef(0.3f, -0.012f, 0); boite(0.2f, 0.006f, 0.008f, 0.35f * l, 0.33f * l, 0.3f * l); glPopMatrix();   /* chaine */
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix();
        glRotatef(pg->arme ? -s * 78.0f : -s * 8.0f, 1, 0, 0);   /* les deux branches levees */
        glTranslatef(0, 0, s * 0.15f);
        boite(0.24f, 0.018f, 0.018f, 0.7f * l, 0.65f * l, 0.58f * l);
        for (int d = -7; d <= 7; d++) {
            glPushMatrix(); glTranslatef(d * 0.032f, 0.04f, 0); boite(0.008f, 0.04f, 0.008f, 0.85f * l, 0.8f * l, 0.72f * l); glPopMatrix();
        }
        glPopMatrix();
    }
    glPopMatrix();
}

/* un autre joueur : silhouette en blouson, lampe a la main (faisceau visible dans la poussiere) */
static const float couleursJ[MAXP][3] = { { 0.25f, 0.3f, 0.45f }, { 0.45f, 0.18f, 0.15f }, { 0.2f, 0.38f, 0.22f }, { 0.4f, 0.35f, 0.15f } };
static void dessiner_joueur(int k, float t) {
    Joueur *j = &JP[k];
    const NetJ *s = &j->st;
    V3 p = j->disp;
    float l = lum_entite(v3(p.x, 1.2f, p.z), t) + 0.15f;   /* un peu de "lumiere de jeu" : on voit ses amis */
    const float *c = couleursJ[k];
    int mort = (s->flags & JF_MORT) != 0, accroupi = (s->flags & JF_ACCROUPI) != 0;
    float h = accroupi ? 0.62f : 1.0f, sw = sinf(s->pas * PI * 0.5f + t * 0) * 22;
    glPushMatrix(); glTranslatef(p.x, 0, p.z); glRotatef(-s->yaw * 180.0f / PI, 0, 1, 0);
    if (mort) { glTranslatef(0, 0.15f, 0); glRotatef(-90, 1, 0, 0); h = 1; }   /* au sol */
    for (int sd = -1; sd <= 1; sd += 2) {                 /* jambes */
        glPushMatrix(); glTranslatef(sd * 0.1f, 0.85f * h, 0); glRotatef(sd * sw, 1, 0, 0);
        membre(0.85f * h, 0.07f, 0.12f * l, 0.12f * l, 0.14f * l); glPopMatrix();
    }
    glPushMatrix(); glTranslatef(0, 0.85f * h + 0.3f, 0); boite(0.2f, 0.3f, 0.12f, c[0] * l, c[1] * l, c[2] * l);   /* blouson */
    glPushMatrix(); glTranslatef(-0.24f, 0.25f, 0); glRotatef(-sw * 0.6f, 1, 0, 0); membre(0.6f, 0.05f, c[0] * l * 0.9f, c[1] * l * 0.9f, c[2] * l * 0.9f); glPopMatrix();
    glPushMatrix(); glTranslatef(0.24f, 0.25f, 0);        /* bras droit : tient la lampe devant */
    glRotatef(-90 - s->lampePitch * 57.3f, 1, 0, 0);
    membre(0.5f, 0.05f, c[0] * l * 0.9f, c[1] * l * 0.9f, c[2] * l * 0.9f);
    glTranslatef(0, -0.55f, 0);
    boite(0.03f, 0.08f, 0.03f, 0.15f * l, 0.15f * l, 0.15f * l);
    glPopMatrix();
    glTranslatef(0, 0.42f, 0); glRotatef(-s->pitch * 40, 1, 0, 0);
    boite(0.1f, 0.12f, 0.11f, 0.55f * l, 0.48f * l, 0.42f * l);   /* tete */
    glTranslatef(0, 0.08f, 0.01f); boite(0.105f, 0.05f, 0.115f, 0.12f * l, 0.1f * l, 0.08f * l);   /* cheveux / bonnet */
    glPopMatrix();
    glPopMatrix();
    if ((s->flags & JF_LAMPE) && !mort) {                 /* faisceau : un cone tres pale, additif */
        V3 o = v3(p.x, s->oeil / 100.0f - 0.15f, p.z), f = view_dir(s->lampeYaw, s->lampePitch);
        V3 r = vnorm(vcross(f, v3(0, 1, 0))), u = vcross(r, f);
        float L = 6, R = L * tanf(LAMPE_ANGLE * PI / 180.0f) * 0.8f;
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); glDepthMask(GL_FALSE);
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(1, 0.95f, 0.8f, 0.10f); glVertex3f(o.x, o.y, o.z);
        glColor4f(1, 0.95f, 0.8f, 0.0f);
        for (int i = 0; i <= 16; i++) {
            float a = i * 2 * PI / 16;
            V3 q = vadd(vadd(o, vmul(f, L)), vadd(vmul(r, cosf(a) * R), vmul(u, sinf(a) * R)));
            glVertex3f(q.x, q.y, q.z);
        }
        glEnd();
        glDepthMask(GL_TRUE); glDisable(GL_BLEND);
        glDisable(GL_FOG);                                /* la lampe elle-meme, qui brille */
        glPushMatrix(); glTranslatef(o.x + f.x * 0.45f, o.y + f.y * 0.45f, o.z + f.z * 0.45f); boite(0.025f, 0.025f, 0.025f, 1, 0.97f, 0.85f); glPopMatrix();
        glEnable(GL_FOG);
    }
}

/* prises, interrupteurs, grilles d'aeration : petits volumes eclaires comme le reste */
static void dessiner_decals(float t) {
    for (int i = 0; i < ndecals; i++) {
        const Decal *d = &decals[i];
        if (fabsf(d->pos.x - camEye.x) > 14 || fabsf(d->pos.z - camEye.z) > 14) continue;
        float l = lum_entite(d->pos, t);
        float yaw = atan2f(d->n.x, d->n.z) * 180.0f / PI;
        glPushMatrix(); glTranslatef(d->pos.x, d->pos.y, d->pos.z); glRotatef(yaw, 0, 1, 0);
        if (d->type == DECAL_PRISE) {
            boite(0.04f, 0.04f, 0.006f, 0.78f * l, 0.74f * l, 0.62f * l);
            for (int s = -1; s <= 1; s += 2) { glPushMatrix(); glTranslatef(s * 0.012f, 0, 0.006f); boite(0.004f, 0.004f, 0.002f, 0.05f, 0.05f, 0.05f); glPopMatrix(); }
        } else if (d->type == DECAL_INTER) {
            boite(0.035f, 0.055f, 0.006f, 0.78f * l, 0.74f * l, 0.62f * l);
            glTranslatef(0, 0.008f, 0.008f); boite(0.01f, 0.018f, 0.004f, 0.7f * l, 0.66f * l, 0.55f * l);
        } else {
            boite(0.22f, 0.11f, 0.008f, 0.45f * l, 0.45f * l, 0.43f * l);
            for (int k = -4; k <= 4; k++) { glPushMatrix(); glTranslatef(0, k * 0.022f, 0.009f); boite(0.2f, 0.006f, 0.003f, 0.08f * l, 0.08f * l, 0.08f * l); glPopMatrix(); }
        }
        glPopMatrix();
    }
}

static void dessiner_entites(float t) {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    fog_scene();
    dessiner_sol(t);
    dessiner_decals(t);
    for (int k = 0; k < MAXP; k++)                        /* les autres (pas ceux qui sont caches) */
        if (netMode != NET_OFF && k != monSlot && JP[k].actif && !(JP[k].st.flags & JF_CACHE)) dessiner_joueur(k, t);
    for (int i = 0; i < nmon; i++) {                      /* les monstres */
        Monstre *m = &mon[i];
        if (!m->actif) continue;
        V3 p = m->pos;
        float l = lum_entite(v3(p.x, 1.4f, p.z), t);
        glPushMatrix(); glTranslatef(p.x, 0, p.z); glRotatef(-m->yaw * 180.0f / PI, 0, 1, 0);
        dessiner_monstre(m->type, m->anim, l, t, m->etat);
        glPopMatrix();
    }
    if (apparitionT >= 0) {                               /* apparition : face a toi, immobile */
        V3 p = apparitionPos;
        float yaw = atan2f(P.pos.x - p.x, -(P.pos.z - p.z));
        float l = lum_entite(v3(p.x, 1.6f, p.z), t) + 0.6f;
        glPushMatrix(); glTranslatef(p.x, 0, p.z); glRotatef(-yaw * 180.0f / PI, 0, 1, 0);
        dessiner_monstre(apparitionType, 0, l, t, E_CHASSE);
        glPopMatrix();
    }
    for (int i = 0; i < npieges; i++) {                   /* pieges */
        Piege *pg = &pieges[i];
        if (pg->type == PIEGE_MACHOIRE) { dessiner_machoire(pg, t); continue; }
        float l = lum_entite(v3(pg->pos.x, 0.3f, pg->pos.z), t);   /* faux fusible : presque pareil qu'un vrai */
        glPushMatrix(); glTranslatef(pg->pos.x, 0.07f, pg->pos.z); glRotatef((float)(i * 53), 0, 1, 0);
        boite(0.14f, 0.07f, 0.09f, 0.45f * l, 0.45f * l, 0.48f * l);
        glTranslatef(0, 0.07f, -0.03f); boite(0.05f, 0.012f, 0.025f, 0.55f * l, 0.15f * l, 0.1f * l);
        float led = pg->arme ? (fmodf(t + i * 0.29f, 1.3f) < 0.12f ? 1.0f : 0.15f) : (fmodf(t, 0.2f) < 0.1f ? 1.0f : 0.2f);
        glTranslatef(0.09f, 0, 0.04f); glDisable(GL_FOG); boite(0.012f, 0.012f, 0.012f, led, led * 0.08f, led * 0.05f); glEnable(GL_FOG);
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

/* screamer : la tete du tueur, collee a la camera (matrice de vue identite). gros : 0 plan serre, 1 tres gros plan */
static void dessiner_screamer(float s, int type, int gros) {
    float shake = (1 - fminf(1, s / SCREAMER_MORT_T)) * 0.04f;
    float flash = fmodf(s, 0.12f) < 0.06f ? 1.7f : 0.85f;
    float z = (gros ? -0.85f : -1.1f) + fminf(s / 0.1f, 1) * 0.55f;   /* elle se jette sur toi en 0,1 s */
    glDisable(GL_FOG); glDisable(GL_TEXTURE_2D);
    glPushMatrix();
    glTranslatef(sinf(s * 91) * shake, (gros ? 0.06f : 0.03f) + cosf(s * 77) * shake, z);
    glRotatef(180, 0, 1, 0);
    glRotatef(sinf(s * 40) * 8 + 12, 0, 0, 1);
    float b = fminf(1, s * 6);
    if (type == MON_LOCATAIRE) tete_locataire(flash, b);
    else if (type == MON_RAMPANTE) tete_rampante(flash, b, 1.6f);
    else tete(flash, b);
    glPopMatrix();
}
