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

/* simule n ticks du joueur avec les actions tenues donnees */
static void test_run(int n, int a1, int a2) {
    for (int i = 0; i < n; i++) {
        memset(botHeld, 0, sizeof botHeld);
        if (a1 >= 0) botHeld[a1] = 1;
        if (a2 >= 0) botHeld[a2] = 1;
        game_tick(TICK);
    }
    memset(botHeld, 0, sizeof botHeld);
}

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
    {
        int okAll = 1;
        for (int s = 0; s < S_COUNT; s++) if (sounds[s].len <= 0 || !sounds[s].data) okAll = 0;
        check("sons : tous fabriques", (float)okAll, 1, 1);
    }

    /* touches par defaut */
    memcpy(binds, bindDefault, sizeof binds);
    check("touches : avancer = W (scancode 0x11)", (float)binds[A_AVANCER][0], SC_W, SC_W);

    /* joueur : vitesses, endurance, collisions */
    reset_game();
    test_run(360, A_AVANCER, -1);
    check("joueur : vitesse de marche (m/s)", hlen(P.vel), MARCHE_V - 0.01f, MARCHE_V + 0.01f);
    reset_game();                                          /* depart z = 7,5 vers -z : 2 s de course sans obstacle */
    test_run(240, A_AVANCER, A_COURIR);
    check("joueur : vitesse de course (m/s)", hlen(P.vel), COURSE_V - 0.05f, COURSE_V + 0.05f);
    reset_game();
    P.pos = P.prevPos = v3(-8, 0, 9.5f); P.yaw = PI / 2;   /* le long du mur du fond, vers +x */
    test_run((int)((ENDURANCE_MAX + 0.5f) / TICK), A_AVANCER, A_COURIR);
    check("joueur : essouffle apres ENDURANCE_MAX de course", (float)P.essouffle, 1, 1);
    check("joueur : a bout de souffle -> marche", hlen(P.vel), 0, MARCHE_V + 0.05f);
    reset_game();
    test_run(360, A_AVANCER, A_ACCROUPI);
    check("joueur : vitesse accroupi (m/s)", hlen(P.vel), ACCROUPI_V - 0.01f, ACCROUPI_V + 0.01f);
    check("joueur : yeux accroupi (m)", P.oeil, OEIL_ACCROUPI - 0.01f, OEIL_ACCROUPI + 0.01f);
    reset_game();
    P.pos = P.prevPos = v3(4.5f, 0, 7.5f); P.yaw = 0;       /* couloir libre entre la cloison et les piliers */
    test_run(120 * 20, A_AVANCER, -1);                     /* 20 s tout droit vers -z : le mur arrete */
    check("joueur : arrete par le mur (z)", P.pos.z, -SALLE_DEMI, -SALLE_DEMI + RAYON_JOUEUR + 0.01f);
    reset_game();
    P.pos = P.prevPos = v3(-6, 0, -3); P.yaw = 0;           /* face au pilier (-6.5..-5.5, -6.5..-5.5) */
    test_run(120 * 5, A_AVANCER, -1);
    check("joueur : arrete par le pilier (z)", P.pos.z, -5.5f + RAYON_JOUEUR - 0.01f, -5.5f + RAYON_JOUEUR + 0.01f);
    reset_game();
    test_run(120 * 5, A_AVANCER, -1);
    check("joueur : pas en 5 s de marche", (float)P.pas, 9, 11);

    fprintf(testOut, "\n%d tests, %d echec(s)\n", testCount, testFails);
    fclose(testOut);
    return testFails ? 1 : 0;
}
