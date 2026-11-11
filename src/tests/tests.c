/* BOURDONNEMENT - captures --shot et tests automatiques --test (meme principe que VELOCITY)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   --test fichier.txt : chaque ligne donne la valeur obtenue et "(attendu ...)" ; code de sortie 1 si un test echoue. */

static void save_bmp(const char *path) {
    int rowSize = (winW * 3 + 3) & ~3;
    unsigned char *px = (unsigned char *)malloc((size_t)rowSize * winH);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, winW, winH, 0x80E0 /* GL_BGR_EXT */, GL_UNSIGNED_BYTE, px);
    FILE *f = fopen(path, "wb");
    if (f) {
        BITMAPFILEHEADER fh = { 0 }; BITMAPINFOHEADER ih = { 0 };
        fh.bfType = 0x4D42; fh.bfOffBits = sizeof fh + sizeof ih; fh.bfSize = fh.bfOffBits + rowSize * winH;
        ih.biSize = sizeof ih; ih.biWidth = winW; ih.biHeight = winH; ih.biPlanes = 1; ih.biBitCount = 24;
        fwrite(&fh, sizeof fh, 1, f); fwrite(&ih, sizeof ih, 1, f); fwrite(px, (size_t)rowSize * winH, 1, f);
        fclose(f);
    }
    free(px);
}

static FILE *testOut;
static int testFails, testCount;
static void check(const char *name, float got, float lo, float hi) {
    int ok = got >= lo && got <= hi;
    testCount++; if (!ok) testFails++;
    fprintf(testOut, "%-48s %10.4f   (attendu %g a %g)%s\n", name, got, lo, hi, ok ? "" : "   <-- ECHEC");
}

/* simule n ticks avec les actions tenues donnees (le Rodeur est mis a l'ecart sauf si rodeur = 1) */
static void test_run(int n, int a1, int a2) {
    for (int i = 0; i < n; i++) {
        memset(botHeld, 0, sizeof botHeld);
        if (a1 >= 0) botHeld[a1] = 1;
        if (a2 >= 0) botHeld[a2] = 1;
        game_tick(TICK);
    }
    memset(botHeld, 0, sizeof botHeld);
}
static void test_isoler(void) { R.actif = 0; R.pos = R.prevPos = v3(-100, 0, -100); }   /* tests du joueur seul */
static void test_place(float x, float z, float yaw) { P.pos = P.prevPos = v3(x, 0, z); P.yaw = P.lampeYaw = yaw; P.vel = v3(0, 0, 0); }

static int run_tests(const char *path) {
    testOut = fopen(path, "w");
    if (!testOut) return 1;
    fprintf(testOut, "BOURDONNEMENT - tests automatiques\n\n");

    /* maths et graine */
    check("maths : |vnorm(3,4,12)|", vlen(vnorm(v3(3, 4, 12))), 0.999f, 1.001f);
    {
        float a[3], b[3];
        rng = 777; for (int i = 0; i < 3; i++) a[i] = frand();
        rng = 777; for (int i = 0; i < 3; i++) b[i] = frand();
        check("graine : meme graine = meme suite", (a[0] == b[0] && a[1] == b[1] && a[2] == b[2]) ? 1.0f : 0.0f, 1, 1);
    }

    /* sons synthetises */
    make_sounds();
    check("sons : bourdon = 2 s exactement (echantillons)", (float)sounds[S_BOURDON].len, 2 * SR, 2 * SR);
    check("sons : bourdon se raccorde (ecart fin/debut)",
          fabsf((float)sounds[S_BOURDON].data[0] - sounds[S_BOURDON].data[sounds[S_BOURDON].len - 1]), 0, 200);
    check("sons : drone se raccorde (ecart fin/debut)",
          fabsf((float)sounds[S_DRONE].data[0] - sounds[S_DRONE].data[sounds[S_DRONE].len - 1]), 0, 300);
    {
        int okAll = 1;
        for (int s = 0; s < S_COUNT; s++) if (sounds[s].len <= 0 || !sounds[s].data) okAll = 0;
        check("sons : tous fabriques", (float)okAll, 1, 1);
    }
    memcpy(binds, bindDefault, sizeof binds);
    check("touches : avancer = W (scancode 0x11)", (float)binds[A_AVANCER][0], SC_W, SC_W);

    /* carte faite a la main */
    reset_game();
    check("carte : largeur x hauteur", (float)(carteW * 100 + carteH), 4834, 4834);
    {
        int libres, n = carte_atteignables(&libres);
        check("carte : toutes les cases libres atteignables", (float)(libres - n), 0, 0);
    }
    check("carte : ascenseur pose, porte vers -x", (float)(asc.actif * 10 + asc.cote), 11, 11);
    check("carte : fusibles", (float)nfus, FUSIBLES_N, FUSIBLES_N);
    check("carte : neons (au moins 60)", (float)nneons, 60, 400);
    check("rayon : depart -> mur a gauche (m)", ray_world(v3(3.5f, 1, 9.5f), v3(-1, 0, 0), 50), 2.49f, 2.51f);
    check("rayon : vers le plafond (m)", ray_world(v3(3.5f, 1, 9.5f), v3(0, 1, 0), 50), HAUT_PLAFOND - 1.01f, HAUT_PLAFOND - 0.99f);

    /* lumiere */
    check("lumiere : precalcul (ms, < 2000)", (float)lmBakeMs, 0, 2000);
    {
        short cand[256]; float c[3], c2[3];
        int k = -1;
        for (int i = 0; i < nneons; i++) if (neons[i].etat == NEON_OK) { k = i; break; }
        V3 sous = v3(neons[k].x, 0, neons[k].z);
        int nc = candidats(sous, cand, 256);
        lum_point(sous, v3(0, 1, 0), cand, nc, -1, c);
        check("lumiere : sol sous un neon (~1)", c[0], 0.7f, 1.6f);
        nc = candidats(v3(44.5f, 0, 4.5f), cand, 256);
        lum_point(v3(44.5f, 0, 4.5f), v3(0, 1, 0), cand, nc, -1, c2);
        check("lumiere : zone noire (presque rien)", c2[0], 0, 0.2f);
        unsigned h1 = 0, h2 = 0;
        for (int i = 0; i < lmTaille * 64; i++) h1 = h1 * 31 + lmFixe[i];
        bake_lightmaps();
        for (int i = 0; i < lmTaille * 64; i++) h2 = h2 * 31 + lmFixe[i];
        check("lumiere : precalcul deterministe", h1 == h2 ? 1.0f : 0.0f, 1, 1);
    }

    /* joueur : vitesses, endurance, collisions (couloir du bas, z = 27,5, x de 1 a 46) */
    reset_game(); test_isoler(); test_place(2.5f, 27.5f, PI / 2);
    test_run(360, A_AVANCER, -1);
    check("joueur : vitesse de marche (m/s)", hlen(P.vel), MARCHE_V - 0.02f, MARCHE_V + 0.02f);
    reset_game(); test_isoler(); test_place(2.5f, 27.5f, PI / 2);
    test_run(240, A_AVANCER, A_COURIR);
    check("joueur : vitesse de course (m/s)", hlen(P.vel), COURSE_V - 0.05f, COURSE_V + 0.05f);
    check("joueur : la course fait du bruit", P.bruit, BRUIT_COURSE - 0.01f, 2);
    reset_game(); test_isoler(); test_place(2.5f, 27.5f, PI / 2);
    test_run((int)((ENDURANCE_MAX + 1.2f) / TICK), A_AVANCER, A_COURIR);
    check("joueur : essouffle apres ENDURANCE_MAX de course", (float)P.essouffle, 1, 1);
    check("joueur : a bout de souffle -> marche", hlen(P.vel), 0, MARCHE_V + 0.05f);
    reset_game(); test_isoler(); test_place(2.5f, 27.5f, PI / 2);
    test_run(360, A_AVANCER, A_ACCROUPI);
    check("joueur : vitesse accroupi (m/s)", hlen(P.vel), ACCROUPI_V - 0.02f, ACCROUPI_V + 0.02f);
    check("joueur : yeux accroupi (m)", P.oeil, OEIL_ACCROUPI - 0.01f, OEIL_ACCROUPI + 0.01f);
    check("joueur : accroupi = presque muet", P.bruit, 0, 0.1f);
    reset_game(); test_isoler(); test_place(40.5f, 27.5f, PI / 2);
    test_run(120 * 10, A_AVANCER, -1);
    check("joueur : arrete par le mur (x)", P.pos.x, 47 - RAYON_JOUEUR - 0.01f, 47 - RAYON_JOUEUR + 0.01f);
    reset_game(); test_isoler(); test_place(2.5f, 27.5f, PI);   /* vers +z : pilier en (2, 28) */
    test_run(120 * 4, A_AVANCER, -1);
    check("joueur : arrete par le pilier (z)", P.pos.z, 28 - RAYON_JOUEUR - 0.01f, 28 - RAYON_JOUEUR + 0.01f);
    reset_game(); test_isoler(); test_place(2.5f, 27.5f, PI / 2);
    test_run(120 * 5, A_AVANCER, -1);
    check("joueur : pas en 5 s de marche", (float)P.pas, 9, 11);
    reset_game(); test_isoler(); test_place(10.5f, 27.5f, PI / 2);
    test_run(60, A_PENCHER_D, -1);
    { float roll; V3 e = player_eye(1, &roll);
      check("joueur : se pencher decale la tete (m)", e.z - P.pos.z, PENCHE_DIST * 0.8f, PENCHE_DIST + 0.01f);
      check("joueur : se pencher incline la vue (deg)", roll, PENCHE_ROULIS * 0.8f, PENCHE_ROULIS + 0.5f); }
    reset_game(); test_isoler();
    { float b0 = P.batterie; test_run(120, -1, -1);
      check("lampe : la batterie s'use (s en 1 s)", b0 - P.batterie, 0.99f, 1.01f);
      botPress[A_LAMPE] = 1; test_run(1, -1, -1);
      check("lampe : s'eteint", (float)P.lampe, 0, 0);
      check("lampe : le clic fait du bruit", P.bruit, 0.2f, 0.3f); }

    /* le Rodeur : ouie */
    reset_game(); R.actif = 1; test_place(10.5f, 27.5f, PI / 2);
    R.pos = R.prevPos = v3(22.5f, 0, 27.5f); rodeur_etat(R_ERRE, 0); rodeur_viser(R.pos);
    P.bruit = BRUIT_ACCROUPI;
    check("rodeur : n'entend pas un pas accroupi a 12 m", (float)rodeur_entend(12), 0, 0);
    P.bruit = BRUIT_COURSE;
    check("rodeur : entend la course a 12 m", (float)rodeur_entend(12), 1, 1);
    reset_game(); R.actif = 1; test_place(10.5f, 27.5f, PI / 2);
    R.pos = R.prevPos = v3(22.5f, 0, 27.5f); rodeur_etat(R_ERRE, 0); rodeur_viser(R.pos);
    test_run(3, A_AVANCER, A_COURIR);                      /* trois ticks de course : il a entendu */
    P.bruit = BRUIT_COURSE; R.entendu = 0;
    for (int i = 0; i < 2; i++) { P.bruit = BRUIT_COURSE; rodeur_tick(TICK); }
    check("rodeur : la course a 12 m -> poursuite", (float)R.etat, R_CHASSE, R_CHASSE);
    {
        int tick = 0;
        for (; tick < 120 * 10 && !P.dead; tick++) { P.bruit = BRUIT_MARCHE; P.pos = P.prevPos = v3(10.5f, 0, 27.5f); game_tick(TICK); }
        check("rodeur : rattrape un joueur immobile qui fait du bruit (s)", tick * TICK, 1, 6);
        check("rodeur : screamer lance", screamT >= 0 ? 1.0f : 0.0f, 1, 1);
    }
    reset_game(); R.actif = 1; test_place(2.5f, 27.5f, PI / 2);
    R.pos = R.prevPos = v3(9.5f, 0, 25.5f); rodeur_etat(R_ERRE, 0); rodeur_viser(R.pos);   /* derriere le mur, a 7 m */
    P.bruit = BRUIT_MARCHE;
    check("rodeur : la marche derriere un mur a 7 m passe", (float)rodeur_entend(7.3f), 0, 0);

    /* objectif et ascenseur */
    reset_game(); test_isoler();
    for (int i = 0; i < nfus; i++) {
        test_place(fus[i].pos.x, fus[i].pos.z, 0);
        botPress[A_UTILISER] = 1; test_run(1, -1, -1);
    }
    check("objectif : 3 fusibles ramasses", (float)fusPris, FUSIBLES_N, FUSIBLES_N);
    check("objectif : l'ascenseur est alimente", (float)asc.alimente, 1, 1);
    test_place(asc.gx + 0.5f + cotes[asc.cote][0] * 1.5f, asc.gz + 0.5f + cotes[asc.cote][1] * 1.5f, 0);
    test_run(120 * 2, -1, -1);
    check("ascenseur : les portes s'ouvrent quand on approche", asc.ouverture, 0.99f, 1);
    {
        P.yaw = atan2f(-(float)cotes[asc.cote][0], (float)cotes[asc.cote][1]);   /* face a la cabine */
        test_run(120 * 3, A_AVANCER, -1);
        int n0 = niveauNum;
        test_run(120 * 5, -1, -1);
        check("ascenseur : on entre, niveau suivant", (float)(niveauNum - n0), 1, 1);
    }

    /* niveaux generes */
    {
        generer_niveau(42, 50, 50);
        int libres, n = carte_atteignables(&libres), ok = 1;
        check("generation : toutes les cases libres atteignables", (float)(libres - n), 0, 0);
        check("generation : assez de place (cases libres)", (float)libres, 500, 2500);
        check("generation : ascenseur et 3 fusibles", (float)(asc.actif * 10 + nfus), 10 + FUSIBLES_N, 10 + FUSIBLES_N);
        carte_distances((int)carteDepart.x, (int)carteDepart.z);
        int ax = asc.gx + cotes[asc.cote][0], az = asc.gz + cotes[asc.cote][1];
        check("generation : ascenseur accessible et loin (cases)", (float)distCase[az][ax], 15, 999);
        for (int i = 0; i < nfus; i++) if (distCase[(int)fus[i].pos.z][(int)fus[i].pos.x] < 0) ok = 0;
        check("generation : fusibles accessibles", (float)ok, 1, 1);
        unsigned h1 = 0; for (int z = 0; z < carteH; z++) for (int x = 0; x < carteW; x++) h1 = h1 * 3 + grille[z][x];
        generer_niveau(42, 50, 50);
        unsigned h2 = 0; for (int z = 0; z < carteH; z++) for (int x = 0; x < carteW; x++) h2 = h2 * 3 + grille[z][x];
        check("generation : meme graine = meme niveau", h1 == h2 ? 1.0f : 0.0f, 1, 1);
        int mauvais = 0;
        for (unsigned g = 1; g <= 200; g++) {
            generer_niveau(g, 40 + g % 40, 40 + g % 40);
            int l2, n2 = carte_atteignables(&l2);
            if (n2 != l2 || !asc.actif || nfus != FUSIBLES_N) {
                mauvais++;
                fprintf(testOut, "    graine %u (%d) : atteignables %d / %d, ascenseur %d, fusibles %d\n", g, 40 + g % 40, n2, l2, asc.actif, nfus);
            }
        }
        check("generation : 200 graines toutes valides", (float)mauvais, 0, 0);
    }

    fprintf(testOut, "\n%d tests, %d echec(s)\n", testCount, testFails);
    fclose(testOut);
    return testFails ? 1 : 0;
}
