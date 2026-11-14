/* BOURDONNEMENT - render() : l'image complete
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */


/* assombrit les bords de l'ecran (vignette), plus fort avec la tension et a chaque battement */
static void vignette(float force) {
    float w = (float)winW, h = (float)winH, b = w * 0.24f;
    rect_vgrad(0, 0, w, h * 0.32f, 0, 0, 0, force, 0);
    rect_vgrad(0, h * 0.68f, w, h * 0.32f, 0, 0, 0, 0, force);
    glBegin(GL_QUADS);                                    /* bords gauche et droit */
    glColor4f(0, 0, 0, force); glVertex2f(0, 0); glColor4f(0, 0, 0, 0); glVertex2f(b, 0); glVertex2f(b, h); glColor4f(0, 0, 0, force); glVertex2f(0, h);
    glColor4f(0, 0, 0, 0); glVertex2f(w - b, 0); glColor4f(0, 0, 0, force); glVertex2f(w, 0); glVertex2f(w, h); glColor4f(0, 0, 0, 0); glVertex2f(w - b, h);
    glEnd();
}

/* filtre de camescope : lignes de balayage, neige, bande de tracking ; force 0..1 */
static void vhs(float t, float force) {
    float w = (float)winW, h = (float)winH;
    glBegin(GL_QUADS);
    glColor4f(0, 0, 0, 0.10f + 0.08f * force);
    for (int y = 0; y < winH; y += 3) { glVertex2f(0, (float)y); glVertex2f(w, (float)y); glVertex2f(w, y + 1.0f); glVertex2f(0, y + 1.0f); }
    unsigned s = (unsigned)(t * 60);
    int nNeige = 120 + (int)(force * 900);
    for (int i = 0; i < nNeige; i++) {                    /* neige */
        unsigned hh = hash32(s * 7919u + (unsigned)i);
        float x = (hh & 4095) / 4095.0f * w, y = ((hh >> 12) & 4095) / 4095.0f * h, a = ((hh >> 24) & 255) / 255.0f;
        glColor4f(1, 1, 1, a * (0.05f + 0.12f * force));
        glVertex2f(x, y); glVertex2f(x + 2, y); glVertex2f(x + 2, y + 1); glVertex2f(x, y + 1);
    }
    float by = fmodf(t * 70.0f, h + 200) - 100;           /* bande de tracking qui descend */
    glColor4f(1, 1, 1, 0.03f + 0.05f * force); glVertex2f(0, by); glVertex2f(w, by); glVertex2f(w, by + 14); glVertex2f(0, by + 14);
    glColor4f(0, 0, 0, 0.12f + 0.15f * force); glVertex2f(0, by + 14); glVertex2f(w, by + 14); glVertex2f(w, by + 20); glVertex2f(0, by + 20);
    glEnd();
}

/* dechirures : barres horizontales blanches / noires, au hasard */
static void dechirures(float t, int n) {
    unsigned s = (unsigned)(t * 40);
    glBegin(GL_QUADS);
    for (int i = 0; i < n; i++) {
        unsigned hh = hash32(s * 104729u + (unsigned)i * 7u);
        float y = (hh & 1023) / 1023.0f * winH, hb = 2.0f + ((hh >> 10) & 31), c = ((hh >> 16) & 1) ? 1.0f : 0.0f;
        glColor4f(c, c * 0.9f, c * 0.9f, 0.5f + ((hh >> 20) & 63) / 140.0f);
        glVertex2f(0, y); glVertex2f((float)winW, y); glVertex2f((float)winW, y + hb); glVertex2f(0, y + hb);
    }
    glEnd();
}

/* pseudos au-dessus des autres joueurs (s'ils sont en vue) */
static void draw_noms(void) {
    for (int k = 0; k < MAXP; k++) {
        if (netMode == NET_OFF || k == monSlot || !JP[k].actif) continue;
        V3 p = v3(JP[k].disp.x, 2.05f, JP[k].disp.z);
        float d = vlen(vsub(p, camEye)), sx, sy;
        if (d > 18 || !visible_xz(camEye.x, camEye.z, p.x, p.z) || !projeter(p, &sx, &sy)) continue;
        float a = clampf(1.4f - d / 12, 0.25f, 1);
        int mort = (JP[k].st.flags & JF_MORT) != 0;
        text_c(0, sx, sy, JP[k].nom, mort ? 0.7f : couleursJ[k][0] + 0.45f, mort ? 0.2f : couleursJ[k][1] + 0.45f, mort ? 0.2f : couleursJ[k][2] + 0.45f, a);
    }
}

/* mini-carte (en haut a gauche) : seulement ce que l'on a deja vu ; jamais les monstres */
static const float couleurZone[ZONE_N][3] = { { 0.55f, 0.5f, 0.3f }, { 0.5f, 0.18f, 0.18f }, { 0.55f, 0.6f, 0.62f }, { 0.35f, 0.45f, 0.6f },
                                              { 0.6f, 0.58f, 0.45f }, { 0.4f, 0.4f, 0.4f }, { 0.35f, 0.42f, 0.3f }, { 0.25f, 0.5f, 0.35f } };
static void draw_minicarte(float t) {
    float S = MINICARTE_TAILLE, x0 = 20, y0 = 60, k = S / MINICARTE_CASES;
    int n = MINICARTE_CASES / 2, px = (int)floorf(P.pos.x), pz = (int)floorf(P.pos.z);
    float d;
    int brouille = mon_proche(&d) >= 0 && d < 7 && (hash32((unsigned)(t * 12)) & 3) == 0;   /* il est la : l'image saute */
    rect(x0 - 3, y0 - 3, S + 6, S + 6, 0, 0, 0, 0.55f);
    rect(x0 - 3, y0 - 3, S + 6, 2, 0.6f, 0.55f, 0.4f, 0.5f);
    float ox = P.pos.x - px, oz = P.pos.z - pz;           /* la carte suit le joueur en douceur */
    glBegin(GL_QUADS);
    for (int dz = -n; dz <= n; dz++)
        for (int dx = -n; dx <= n; dx++) {
            int x = px + dx, z = pz + dz;
            if (x < 0 || z < 0 || x >= carteW || z >= carteH) continue;
            int vu = decouvert[z][x];
            if (!vu && cell_mur(x, z))                    /* un mur se dessine si une case voisine a ete vue */
                for (int c = 0; c < 4 && !vu; c++) { int nx = x + cotes[c][0], nz = z + cotes[c][1]; vu = nx >= 0 && nz >= 0 && nx < carteW && nz < carteH && decouvert[nz][nx]; }
            if (!vu) continue;
            float sx = x0 + S * 0.5f + (dx - ox) * k, sy = y0 + S * 0.5f + (dz - oz) * k;
            if (sx < x0 - 0.01f || sy < y0 - 0.01f || sx + k > x0 + S + 0.01f || sy + k > y0 + S + 0.01f) continue;
            const float *c = couleurZone[zone[z][x]];
            float a = 0.55f;
            if (cell_opaque(x, z)) glColor4f(0.12f, 0.1f, 0.07f, 0.9f);
            else if (cell_muret(x, z)) glColor4f(0.3f, 0.32f, 0.35f, 0.9f);
            else if (sol[z][x] == SOL_EAU) glColor4f(0.15f, 0.2f, 0.28f, a + 0.2f);
            else glColor4f(c[0], c[1], c[2], a);
            if (brouille) { float g = (hash32((unsigned)(x * 7 + z * 13) + (unsigned)(t * 30)) & 255) / 255.0f; glColor4f(g, g, g, 0.5f); }
            glVertex2f(sx, sy); glVertex2f(sx + k, sy); glVertex2f(sx + k, sy + k); glVertex2f(sx, sy + k);
        }
    glEnd();
    float cx = x0 + S * 0.5f, cy = y0 + S * 0.5f;
    for (int i = 0; i < nfus; i++) {                      /* fusibles deja vus */
        if (fus[i].pris || !decouvert[(int)fus[i].pos.z][(int)fus[i].pos.x]) continue;
        float sx = cx + (fus[i].pos.x - P.pos.x) * k, sy = cy + (fus[i].pos.z - P.pos.z) * k;
        if (fabsf(sx - cx) < S * 0.5f - 3 && fabsf(sy - cy) < S * 0.5f - 3) rect(sx - 3, sy - 3, 6, 6, 0.95f, 0.15f, 0.1f, fmodf(t, 1.0f) < 0.6f ? 1.0f : 0.5f);
    }
    if (asc.actif && decouvert[asc.gz][asc.gx]) {         /* l'ascenseur : rouge sans courant, vert avec */
        float sx = cx + (asc.gx + 0.5f - P.pos.x) * k, sy = cy + (asc.gz + 0.5f - P.pos.z) * k;
        if (fabsf(sx - cx) < S * 0.5f - 4 && fabsf(sy - cy) < S * 0.5f - 4) rect(sx - 4, sy - 4, 8, 8, asc.alimente ? 0.2f : 0.9f, asc.alimente ? 0.9f : 0.2f, 0.2f, 1);
    }
    for (int j = 0; j < MAXP; j++) {                      /* les autres joueurs */
        if (netMode == NET_OFF || j == monSlot || !JP[j].actif || (JP[j].st.flags & JF_MORT)) continue;
        float sx = cx + (JP[j].disp.x - P.pos.x) * k, sy = cy + (JP[j].disp.z - P.pos.z) * k;
        if (fabsf(sx - cx) < S * 0.5f - 3 && fabsf(sy - cy) < S * 0.5f - 3) rect(sx - 3, sy - 3, 6, 6, couleursJ[j][0] + 0.4f, couleursJ[j][1] + 0.4f, couleursJ[j][2] + 0.4f, 1);
    }
    V3 f = view_dir(P.yaw, 0), r = v3(-f.z, 0, f.x);      /* toi : une fleche */
    glColor4f(1, 0.95f, 0.75f, 1);
    glBegin(GL_TRIANGLES);
    glVertex2f(cx + f.x * 7, cy + f.z * 7);
    glVertex2f(cx - f.x * 4 + r.x * 4, cy - f.z * 4 + r.z * 4);
    glVertex2f(cx - f.x * 4 - r.x * 4, cy - f.z * 4 - r.z * 4);
    glEnd();
}

static void draw_hud(float t) {
    char b[96];
    draw_noms();
    if (miniCarte) draw_minicarte(t);
    if (fusPris < nfus) snprintf(b, sizeof b, "FUSIBLES  %d / %d", fusPris, nfus);   /* objectif, discret */
    else snprintf(b, sizeof b, "L'ASCENSEUR");
    text(0, miniCarte ? 20.0f : 28.0f, miniCarte ? 60 + MINICARTE_TAILLE + 22 : 44, b, 0.75f, 0.72f, 0.6f, 0.6f);
    /* coeur : BPM facon camescope, carre rouge qui bat */
    float pp = coeurPouls;
    rect(winW - 160.0f - pp * 2, 36.0f - pp * 2, 12 + pp * 4, 12 + pp * 4, 0.8f, 0.05f, 0.05f, 0.5f + 0.5f * pp);
    snprintf(b, sizeof b, "%3d BPM", (int)(bpm + 0.5f));
    text(0, winW - 138.0f, 48, b, 0.9f, bpm > 140 ? 0.3f : 0.85f, bpm > 140 ? 0.25f : 0.75f, 0.85f);
    /* bruit : ce qu'il peut entendre */
    float br = clampf(P.bruit / 1.2f, 0, 1);
    text(0, 28, winH - 52.0f, "BRUIT", 0.6f, 0.58f, 0.5f, 0.5f);
    rect(28, winH - 42.0f, 110, 4, 0.2f, 0.2f, 0.2f, 0.4f);
    rect(28, winH - 42.0f, 110 * br, 4, 0.5f + 0.5f * br, 0.6f - 0.4f * br, 0.4f - 0.3f * br, 0.75f);
    if (micOuvert) {                                      /* ta voix : au-dessus du trait, il t'entend */
        float mv = clampf(micNiveau * micGain / (MICRO_SEUIL + MICRO_PLAGE * MICRO_MAX), 0, 1), sl = MICRO_SEUIL / (MICRO_SEUIL + MICRO_PLAGE * MICRO_MAX);
        text(0, 150, winH - 52.0f, "MICRO", 0.6f, 0.58f, 0.5f, 0.5f);
        rect(150, winH - 42.0f, 80, 4, 0.2f, 0.2f, 0.2f, 0.4f);
        rect(150, winH - 42.0f, 80 * mv, 4, mv > sl ? 0.95f : 0.4f, mv > sl ? 0.2f : 0.7f, mv > sl ? 0.1f : 0.4f, 0.8f);
        rect(150 + 80 * sl, winH - 45.0f, 1, 10, 1, 1, 1, 0.5f);
    }
    /* batterie de la lampe */
    float bat = P.batterie / LAMPE_BATTERIE;
    int faible = bat < LAMPE_FAIBLE;
    if (!faible || fmodf(t, 0.8f) < 0.5f) {
        rect(winW - 92.0f, winH - 44.0f, 60, 12, 0.6f, 0.6f, 0.55f, 0.35f);
        rect(winW - 32.0f, winH - 41.0f, 4, 6, 0.6f, 0.6f, 0.55f, 0.35f);
        rect(winW - 90.0f, winH - 42.0f, 56 * bat, 8, faible ? 0.9f : 0.75f, faible ? 0.2f : 0.72f, faible ? 0.1f : 0.5f, 0.7f);
    }
    float e = P.endurance / ENDURANCE_MAX;                /* endurance : seulement quand elle n'est pas pleine */
    if (e < 0.999f) rect(winW * 0.5f - 60, winH - 30.0f, 120 * e, 3, P.essouffle ? 0.8f : 0.85f, P.essouffle ? 0.2f : 0.8f,
                         P.essouffle ? 0.15f : 0.6f, 0.5f);
    if (P.piegeT > 0) text_c(1, winW * 0.5f, winH * 0.5f + 70, "[E]  [E]  [E]", 0.9f, 0.2f, 0.15f, fmodf(t, 0.3f) < 0.15f ? 1.0f : 0.6f);
    else if (P.cache == 2) text_c(0, winW * 0.5f, winH - 120.0f, "[E]  sortir de l'armoire", 0.6f, 0.56f, 0.48f, 0.7f);
    else if (P.cache == 1) text_c(0, winW * 0.5f, winH - 120.0f, "cache sous le lit - ne fais pas de bruit", 0.6f, 0.56f, 0.48f, 0.7f);
    else if (objet_vise() >= 0) text_c(0, winW * 0.5f, winH * 0.5f + 60, "[E]  ramasser le fusible", 0.85f, 0.82f, 0.7f, 0.8f);
    else if (meuble_vise_de(P.pos, P.yaw, MB_CASSETTE) >= 0) text_c(0, winW * 0.5f, winH * 0.5f + 60, "[E]  prendre la cassette", 0.85f, 0.82f, 0.7f, 0.8f);
    else if (armoire_visee() >= 0) text_c(0, winW * 0.5f, winH * 0.5f + 60, "[E]  se cacher dans l'armoire", 0.85f, 0.82f, 0.7f, 0.8f);
    if (msgT > 0) {
        int radioMsg = msgTexte[0] == 'R';
        text_c(0, winW * 0.5f, winH - 90.0f, msgTexte, radioMsg ? 0.6f : 0.92f, radioMsg ? 0.85f : 0.88f, radioMsg ? 0.6f : 0.72f, fminf(1, msgT));
    }
}

/* carton : texte tape a la machine, centre, sur fond noir qui grezille */
static void draw_carton(float t) {
    rect(0, 0, (float)winW, (float)winH, 0, 0, 0, 1);
    vhs(t, 0.25f);
    int n = 0; while (cartonL[n]) n++;
    float lh = 30, y = winH * 0.5f - n * lh * 0.5f;
    if (cartonTitre[0]) text_c(1, winW * 0.5f, y - 50, cartonTitre, 0.92f, 0.86f, 0.55f, fminf(1, cartonT * 2));
    int reste = (int)cartonCars;
    char ligne[256];
    for (int i = 0; i < n; i++) {
        int L = (int)strlen(cartonL[i]);
        int k = reste < L ? (reste < 0 ? 0 : reste) : L;
        snprintf(ligne, sizeof ligne, "%.*s", k, cartonL[i]);
        int curseur = reste >= 0 && reste < L + 8 && fmodf(t, 0.6f) < 0.35f;
        if (curseur && k < (int)sizeof ligne - 2) { ligne[k] = '_'; ligne[k + 1] = 0; }
        const char *c0 = cartonL[i];
        while (*c0 == ' ') c0++;
        int cri = (unsigned char)*c0 == 0xAB || (*c0 >= 'A' && *c0 <= 'Z' && c0[1] >= 'A' && c0[1] <= 'Z');   /* citations et cris */
        text_c(0, winW * 0.5f, y + i * lh, ligne, cri ? 0.88f : 0.82f, cri ? 0.32f : 0.8f, cri ? 0.28f : 0.72f, 1);
        reste -= L + 8;
    }
    if (cartonCars >= cartonTotal && fmodf(t, 1.0f) < 0.6f)
        text_c(0, winW * 0.5f, winH - 60.0f, "[E] continuer", 0.6f, 0.58f, 0.5f, 0.8f);
}

/* mort en ligne : la camera se place derriere l'epaule de celui que l'on regarde, sans traverser les murs */
static V3 spectateur_oeil(const Joueur *j, float yaw, float pitch) {
    V3 tete = v3(j->disp.x, j->st.oeil / 100.0f + 0.3f, j->disp.z), arr = vmul(view_dir(yaw, pitch), -1), eye = tete;
    for (float d = 0.1f; d <= SPECTATEUR_RECUL; d += 0.05f) {
        V3 q = vadd(tete, vmul(arr, d));
        if (q.y < 0.25f || q.y > HAUT_PLAFOND - 0.15f) break;
        int bloque = 0;                                   /* garder un peu de marge avec les murs */
        for (int c = 0; c < 4 && !bloque; c++)
            bloque = cell_mur((int)floorf(q.x + (c & 1 ? 0.15f : -0.15f)), (int)floorf(q.z + (c & 2 ? 0.15f : -0.15f)));
        if (bloque) break;
        eye = q;
    }
    return eye;
}

static void render(float alpha, float frameDt) {
    float t = appState == APP_GAME ? gameT : (float)nowT;
    cctv_maj(t, frameDt);                                 /* les ecrans de la salle des cameras (seulement si on est pres) */
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (appState == APP_GAME && cartonActif) {            /* histoire : rien d'autre a l'ecran */
        begin_2d();
        draw_carton((float)nowT);
        if (!captured && !shotMode) draw_pause();
        glEnable(GL_DEPTH_TEST);
        return;
    }
    V3 eye;
    float yaw, pitch, roll = 0, fov = FOV_V;
    if (appState == APP_GAME && spectateur() >= 0) {      /* mort en ligne : on regarde par les yeux d'un autre */
        Joueur *j = &JP[spectateur()];
        yaw = j->st.yaw; pitch = j->st.pitch;
        eye = spectateur_oeil(j, yaw, pitch);
        lampeK = 0;
    } else if (appState == APP_GAME) {
        eye = player_eye(alpha, &roll);
        yaw = P.yaw; pitch = P.pitch;
        fov += FOV_COURSE * P.courseK - coeurPouls * clampf((bpm - 60) / 130.0f, 0, 1) * 1.8f;   /* la vue bat avec le coeur */
        if (tension > 0.6f) roll += sinf(t * 2.1f) * (tension - 0.6f) * 2;   /* poursuite : la vue tangue */
        if (P.piegeT > 0) { pitch -= 0.35f; roll += sinf(t * 25) * 2; }   /* pris : on regarde sa jambe */
        lampeK = lampe_intensite(t);
    } else {                                              /* menus : on regarde le hall, sans lampe */
        eye = v3(carteDepart.x, 1.6f, carteDepart.z);
        yaw = carteDepartYaw + sinf(t * 0.07f) * 0.5f; pitch = -0.05f + sinf(t * 0.05f) * 0.04f;
        lampeK = 0;
    }
    if (screamT >= 0) {                                   /* screamer : tout tremble */
        float s = 1 - fminf(1, screamT / SCREAMER_MORT_T);
        yaw += sinf(t * 83) * 0.07f * s; pitch += cosf(t * 71) * 0.07f * s; roll += sinf(t * 97) * 9 * s;
    }
    camEye = eye;
    set_camera(eye, yaw, pitch, roll, fov);
    glEnable(GL_DEPTH_TEST);
    float dy = P.lampeYaw - P.yaw;
    while (dy > PI) dy -= 2 * PI;
    while (dy < -PI) dy += 2 * PI;
    monde_draw(t, appState == APP_GAME ? lampeK : 0, dy, P.lampePitch - P.pitch);
    if (appState == APP_GAME) { dessiner_entites(t); dessiner_meubles(t); }

    int plan = -1;                                        /* screamer : coupes toutes les 90 ms */
    if (screamT >= 0) plan = screamT < 0.06f ? 9 : (int)(hash32((unsigned)(screamT / 0.09f) * 2654435761u) % 4);
    if (subliminalT >= 0) plan = 1;
    if (!flashsOn && plan >= 0 && screamT < 0) plan = -1;   /* sans flashs : pas d'image subliminale */
    if (!flashsOn && plan == 2) plan = 0;                  /* ni negatif */
    if (plan >= 0) {
        begin_2d();
        int negatif = plan == 2;
        if (plan == 9) rect(0, 0, (float)winW, (float)winH, 0, 0, 0, 1);   /* une image noire, juste avant */
        else rect(0, 0, (float)winW, (float)winH, negatif ? 0.9f : 0.07f, negatif ? 0.88f : 0.0f, negatif ? 0.85f : 0.0f, 0.93f);
        if (plan != 9) {
            glEnable(GL_DEPTH_TEST); glDisable(GL_BLEND);
            glClear(GL_DEPTH_BUFFER_BIT);
            set_camera(v3(0, 0, 0), 0, 0, sinf(t * 97) * 5, FOV_V);
            int type = subliminalT >= 0 ? (nmon > 0 ? mon[0].type : MON_RODEUR) : screamType;
            if (negatif) { glEnable(GL_COLOR_LOGIC_OP); glLogicOp(GL_COPY_INVERTED); }
            dessiner_screamer(subliminalT >= 0 ? 0.5f : screamT, type, plan == 1 || plan == 3);
            if (negatif) glDisable(GL_COLOR_LOGIC_OP);
        }
    }

    begin_2d();
    if (appState == APP_GAME && P.cache == 2 && !P.dead) {   /* dans l'armoire : tout est noir, sauf la fente */
        float fw = winW * 0.07f, cx = winW * 0.5f;
        rect(0, 0, cx - fw * 0.5f, (float)winH, 0.01f, 0.008f, 0.005f, 0.985f);
        rect(cx + fw * 0.5f, 0, winW - cx - fw * 0.5f, (float)winH, 0.01f, 0.008f, 0.005f, 0.985f);
        rect(cx - fw * 0.5f - 6, 0, 6, (float)winH, 0.12f, 0.07f, 0.04f, 0.9f);
        rect(cx + fw * 0.5f, 0, 6, (float)winH, 0.12f, 0.07f, 0.04f, 0.9f);
    } else if (appState == APP_GAME && P.cache == 1 && !P.dead) {   /* sous le lit : le sommier au-dessus du nez */
        rect_vgrad(0, 0, (float)winW, winH * 0.42f, 0.02f, 0.015f, 0.01f, 0.95f, 0.75f);
        for (int k = 0; k < 6; k++) rect(0, winH * (0.03f + k * 0.065f), (float)winW, 7, 0.16f, 0.1f, 0.05f, 0.85f);   /* lattes */
        rect_vgrad(0, winH * 0.42f, (float)winW, winH * 0.1f, 0.02f, 0.015f, 0.01f, 0.75f, 0);
    }
    float pouls = coeurPouls * clampf((bpm - 80) / 100.0f, 0, 1);
    vignette(0.6f + 0.3f * tension + 0.25f * pouls);
    if (appState == APP_MENU) { vhs((float)nowT, 0.1f); draw_title(t); }
    else if (appState == APP_EN_LIGNE) { vhs((float)nowT, 0.1f); draw_en_ligne(); draw_statut(); }
    else {
        if (tension > 0.3f) {                             /* poursuite : les bords battent en rouge sombre */
            float p = (tension - 0.3f) * (0.4f + 0.6f * coeurPouls);
            rect_vgrad(0, 0, (float)winW, winH * 0.2f, 0.4f, 0, 0, p * 0.4f, 0);
            rect_vgrad(0, winH * 0.8f, (float)winW, winH * 0.2f, 0.4f, 0, 0, 0, p * 0.4f);
        }
        vhs(t, tension * 0.6f + (screamT >= 0 ? 1 : 0));
        if (screamT >= 0) {
            dechirures(t, 6 + (int)(hash32((unsigned)(t * 30)) % 14));
            float f = fmodf(screamT, 0.1f) < 0.05f ? 1.0f : 0.0f;
            if (screamT < 0.6f && flashsOn) rect(0, 0, (float)winW, (float)winH, 0.55f * f + 0.05f, 0, 0, 0.3f);   /* flashs rouge sang */
            if (screamT > SCREAMER_MORT_T - 0.4f) rect(0, 0, (float)winW, (float)winH, 0, 0, 0, (screamT - (SCREAMER_MORT_T - 0.4f)) * 2.5f);
        } else if (mortT >= 0 && spectateur() >= 0) {   /* en ligne : on regarde les autres */
            draw_noms();
            char b[80]; snprintf(b, sizeof b, "TU OBSERVES %s   -   [E] suivant", JP[spectateur()].nom);
            text_c(0, winW * 0.5f, winH - 70.0f, b, 0.75f, 0.3f, 0.25f, 0.9f);
            text_c(0, winW * 0.5f, winH - 44.0f, mortT > REAPPARITION_DELAI ? "[ESPACE] revenir au depart" : "le niveau recommence si tout le monde est pris", 0.5f, 0.46f, 0.4f, 0.8f);
        } else if (mortT >= 0) {
            rect(0, 0, (float)winW, (float)winH, 0, 0, 0, 1);
            vhs(t, 0.5f);
            if (fmodf(mortT, 2.7f) < 0.05f) dechirures(t, 10);
            float a = fminf(1, mortT);
            text_c(2, winW * 0.5f, winH * 0.42f, texteMort[screamType], 0.75f, 0.08f, 0.05f, a);
            text_c(0, winW * 0.5f, winH * 0.42f + 50, texteConseil[screamType], 0.55f, 0.5f, 0.45f, a * 0.8f);
            if (mortT > 1.5f) text_c(0, winW * 0.5f, winH * 0.42f + 90, netMode == NET_OFF ? "[E] recommencer le niveau" : mortT > REAPPARITION_DELAI ? "[ESPACE] revenir au depart" : "les autres sont encore la...", 0.6f, 0.55f, 0.5f, a * 0.8f);
        } else {
            if (subliminalT >= 0) dechirures(t, 8);
            draw_vhs_overlay(t);
            draw_hud(t);
            if (sortieT > ASC_PORTE_T) rect(0, 0, (float)winW, (float)winH, 0, 0, 0, fminf(1, (sortieT - ASC_PORTE_T) / 1.5f));
            if (!captured && !shotMode) draw_pause();
        }
    }
    glEnable(GL_DEPTH_TEST);
}
