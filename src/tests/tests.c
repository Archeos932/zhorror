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
static void test_isoler(void) {                          /* tests du joueur seul : ni monstre ni piege */
    monstres_reset();
    for (int i = 0; i < npieges; i++) pieges[i].arme = 0;
}
static void test_reset(void) { reset_game(); cartonActif = 0; }   /* sans les textes de l'histoire */
static void test_monstre(int type, float x, float z) {
    test_isoler(); mon_ajouter(type, 1);
    monstresEveilles = 1;
    mon[0].pos = mon[0].prevPos = v3(x, 0, z); mon_etat(&mon[0], E_ERRE, 0); mon_viser(&mon[0], mon[0].pos);
}
static void test_place(float x, float z, float yaw) { P.pos = P.prevPos = v3(x, 0, z); P.yaw = P.lampeYaw = yaw; P.vel = v3(0, 0, 0); }

static int run_tests(const char *path) {
    testOut = fopen(path, "w");
    if (!testOut) return 1;
    fprintf(testOut, "BOURDONNEMENT - tests automatiques\n\n");
    carteTest = 1;                                        /* coordonnees connues : la carte d'essai */

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
    {
        int noms = 1;
        for (int i = 0; i < A_COUNT; i++) if (!actId[i] || !actId[i][0] || !actLabel[i] || !actLabel[i][0]) noms = 0;
        check("touches : chaque action a un nom et un libelle", (float)noms, 1, 1);
    }

    /* histoire */
    reset_game();
    check("histoire : le jeu s'ouvre sur le texte d'ouverture", (float)(cartonActif && cartonL == texteIntro), 1, 1);
    botPress[A_UTILISER] = 1; test_run(1, -1, -1);
    botPress[A_UTILISER] = 1; test_run(1, -1, -1);
    check("histoire : puis le carton du niveau 0", (float)(cartonActif && cartonL == texteNiveaux[0]), 1, 1);
    { float x0 = P.pos.x; test_run(120, A_AVANCER, -1);
      check("histoire : le monde attend pendant le texte (m)", P.pos.x - x0, 0, 0); }
    botPress[A_UTILISER] = 1; test_run(1, -1, -1);
    botPress[A_UTILISER] = 1; test_run(1, -1, -1);
    check("histoire : E ferme le carton", (float)cartonActif, 0, 0);
    { int ok = 1; for (int n = 1; n <= 9; n++) if (!texte_niveau(n)[0] || !radio(n, 2)) ok = 0;
      check("histoire : un texte et une radio pour chaque niveau", (float)ok, 1, 1); }

    /* carte faite a la main */
    test_reset();
    check("carte : largeur x hauteur", (float)(carteW * 100 + carteH), 4834, 4834);
    {
        int libres, n = carte_atteignables(&libres);
        check("carte : toutes les cases libres atteignables", (float)(libres - n), 0, 0);
    }
    check("carte : ascenseur pose, porte vers -x", (float)(asc.actif * 10 + asc.cote), 11, 11);
    check("carte : fusibles", (float)nfus, FUSIBLES_N, FUSIBLES_N);
    check("carte : machoires de loup", (float)npieges, 3, 3);
    check("carte : flaque dans le couloir des neons fous", (float)sol_en(10.5f, 16.5f), SOL_EAU, SOL_EAU);
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
    test_reset(); test_isoler(); test_place(2.5f, 27.5f, PI / 2);
    test_run(360, A_AVANCER, -1);
    check("joueur : vitesse de marche (m/s)", hlen(P.vel), MARCHE_V - 0.02f, MARCHE_V + 0.02f);
    test_reset(); test_isoler(); test_place(2.5f, 27.5f, PI / 2);
    test_run(240, A_AVANCER, A_COURIR);
    check("joueur : vitesse de course (m/s)", hlen(P.vel), COURSE_V - 0.05f, COURSE_V + 0.05f);
    check("joueur : la course fait du bruit", P.bruit, BRUIT_COURSE - 0.01f, 2);
    test_reset(); test_isoler(); test_place(2.5f, 27.5f, PI / 2);
    test_run((int)((ENDURANCE_MAX + 1.2f) / TICK), A_AVANCER, A_COURIR);
    check("joueur : essouffle apres ENDURANCE_MAX de course", (float)P.essouffle, 1, 1);
    check("joueur : a bout de souffle -> marche", hlen(P.vel), 0, MARCHE_V + 0.05f);
    test_reset(); test_isoler(); test_place(2.5f, 27.5f, PI / 2);
    test_run(360, A_AVANCER, A_ACCROUPI);
    check("joueur : vitesse accroupi (m/s)", hlen(P.vel), ACCROUPI_V - 0.02f, ACCROUPI_V + 0.02f);
    check("joueur : yeux accroupi (m)", P.oeil, OEIL_ACCROUPI - 0.01f, OEIL_ACCROUPI + 0.01f);
    check("joueur : accroupi = presque muet", P.bruit, 0, 0.1f);
    test_reset(); test_isoler(); test_place(40.5f, 27.5f, PI / 2);
    test_run(120 * 10, A_AVANCER, -1);
    check("joueur : arrete par le mur (x)", P.pos.x, 47 - RAYON_JOUEUR - 0.01f, 47 - RAYON_JOUEUR + 0.01f);
    test_reset(); test_isoler(); test_place(2.5f, 27.5f, PI);   /* vers +z : pilier en (2, 28) */
    test_run(120 * 4, A_AVANCER, -1);
    check("joueur : arrete par le pilier (z)", P.pos.z, 28 - RAYON_JOUEUR - 0.01f, 28 - RAYON_JOUEUR + 0.01f);
    test_reset(); test_isoler(); test_place(2.5f, 27.5f, PI / 2);
    test_run(120 * 5, A_AVANCER, -1);
    check("joueur : pas en 5 s de marche", (float)P.pas, 13, 15);
    test_reset(); test_isoler(); test_place(10.5f, 27.5f, PI / 2);
    test_run(60, A_PENCHER_D, -1);
    { float roll; V3 e = player_eye(1, &roll);
      check("joueur : se pencher decale la tete (m)", e.z - P.pos.z, PENCHE_DIST * 0.8f, PENCHE_DIST + 0.01f);
      check("joueur : se pencher incline la vue (deg)", roll, PENCHE_ROULIS * 0.8f, PENCHE_ROULIS + 0.5f); }
    test_reset(); test_isoler();
    { float b0 = P.batterie; test_run(120, -1, -1);
      check("lampe : la batterie s'use (s en 1 s)", b0 - P.batterie, 0.99f, 1.01f);
      botPress[A_LAMPE] = 1; test_run(1, -1, -1);
      check("lampe : s'eteint", (float)P.lampe, 0, 0);
      check("lampe : le clic fait du bruit", P.bruit, 0.2f, 0.3f); }

    /* sol et pieges */
    test_reset(); test_isoler(); test_place(9.2f, 16.5f, PI / 2);   /* la flaque du couloir */
    { float bmax = 0; for (int i = 0; i < 240; i++) { test_run(1, A_AVANCER, -1); bmax = fmaxf(bmax, P.bruit); }
      check("sol : marcher dans l'eau fait du bruit", bmax, BRUIT_EAU - 0.05f, 2); }
    test_reset(); test_isoler(); test_place(9.2f, 16.5f, PI / 2);
    { float bmax = 0; for (int i = 0; i < 360; i++) { test_run(1, A_AVANCER, A_ACCROUPI); bmax = fmaxf(bmax, P.bruit); }
      check("sol : accroupi dans l'eau, moins", bmax, 0.2f, BRUIT_EAU * 0.5f); }
    test_reset(); monstres_reset(); test_place(30.5f, 27.5f, PI / 2);   /* la machoire en (33, 27) */
    test_run(120 * 3, A_AVANCER, -1);
    check("piege : la machoire attrape (x)", P.pos.x, 32.9f, 33.4f);
    check("piege : immobilise", P.piegeT, 0.5f, MACHOIRE_T);
    check("piege : le coeur bondit (BPM)", bpm, BPM_REPOS + 30, BPM_MAX);
    P.piegeT = MACHOIRE_T;
    { int n = 0; while (P.piegeT > 0 && n < 120 * 10) { if (n % 12 == 0) botPress[A_UTILISER] = 1; test_run(1, -1, -1); n++; }
      check("piege : marteler E libere plus vite (s)", n * TICK, 0.4f, MACHOIRE_T * 0.5f); }
    test_reset(); test_isoler(); test_place(10.5f, 27.5f, PI / 2);
    add_piege(11, 27, PIEGE_ALARME);
    botPress[A_UTILISER] = 1; test_run(2, -1, -1);
    check("piege : le faux fusible sonne (bruit dans le monde)", bruitMondeForce, BRUIT_ALARME - 0.1f, BRUIT_ALARME);
    check("piege : ce n'est pas un vrai fusible", (float)fusPris, 0, 0);

    /* le Rodeur : ouie */
    test_reset(); test_place(10.5f, 27.5f, PI / 2); test_monstre(MON_RODEUR, 22.5f, 27.5f);
    P.bruit = BRUIT_ACCROUPI;
    check("rodeur : n'entend pas un pas accroupi a 12 m", (float)mon_entend(&mon[0], 12), 0, 0);
    P.bruit = BRUIT_COURSE;
    check("rodeur : entend la course a 12 m", (float)mon_entend(&mon[0], 12), 1, 1);
    for (int i = 0; i < 2; i++) { P.bruit = BRUIT_COURSE; monstres_tick(TICK, 0); }
    check("rodeur : la course a 12 m -> poursuite", (float)mon[0].etat, E_CHASSE, E_CHASSE);
    {
        int tick = 0;
        for (; tick < 120 * 10 && !P.dead; tick++) { P.bruit = BRUIT_MARCHE; P.pos = P.prevPos = v3(10.5f, 0, 27.5f); game_tick(TICK); }
        check("rodeur : rattrape un joueur immobile qui fait du bruit (s)", tick * TICK, 1, 5);
        check("rodeur : screamer lance, tete du Rodeur", screamT >= 0 && screamType == MON_RODEUR ? 1.0f : 0.0f, 1, 1);
    }
    test_reset(); test_place(2.5f, 27.5f, PI / 2); test_monstre(MON_RODEUR, 9.5f, 25.5f);   /* derriere le mur, a 7 m */
    P.bruit = BRUIT_MARCHE;
    check("rodeur : la marche derriere un mur a 7 m passe", (float)mon_entend(&mon[0], 7.3f), 0, 0);
    test_reset(); test_place(2.5f, 27.5f, PI / 2); test_monstre(MON_RODEUR, 20.5f, 27.5f);
    bruit_monde(v3(14.5f, 0, 27.5f), BRUIT_ALARME);
    monstres_tick(TICK, 0);
    check("rodeur : une alarme l'attire", (float)mon[0].etat, E_ENQUETE, E_ENQUETE);

    /* le Locataire : fige dans la lumiere */
    test_reset(); test_place(39.5f, 7.5f, PI / 2); test_monstre(MON_LOCATAIRE, 45.5f, 7.5f);
    mon[0].vivT = LOCATAIRE_REVEIL + 1;
    check("locataire : la zone est sombre", lum_case(45.5f, 7.5f), 0, LOCATAIRE_LUMIERE);
    test_run(60, -1, -1);
    check("locataire : dans le faisceau, il est fige", (float)mon[0].etat, E_FIGE, E_FIGE);
    check("locataire : ... et n'a pas bouge (m)", dist_xz(mon[0].pos, v3(45.5f, 0, 7.5f)), 0, 0.01f);
    botPress[A_LAMPE] = 1; test_run(120, -1, -1);
    check("locataire : lampe eteinte, il avance (m en 1 s)", 6.0f - dist_xz(mon[0].pos, P.pos), 1.5f, 4);

    /* la Rampante : fonce sur la lumiere */
    test_reset(); test_place(2.5f, 27.5f, PI / 2); test_monstre(MON_RAMPANTE, 18.5f, 27.5f);
    test_run(2, -1, -1);
    check("rampante : voit la lampe a 16 m -> poursuite", (float)mon[0].etat, E_CHASSE, E_CHASSE);
    botPress[A_LAMPE] = 1; test_run((int)((RAMPANTE_OUBLI + 0.3f) / TICK), -1, -1);
    check("rampante : lampe eteinte, elle te perd", mon[0].etat != E_CHASSE ? 1.0f : 0.0f, 1, 1);
    test_reset(); test_place(2.5f, 27.5f, PI / 2); test_monstre(MON_RAMPANTE, 18.5f, 27.5f);
    {
        int tick = 0;
        for (; tick < 120 * 8 && !P.dead; tick++) { P.pos = P.prevPos = v3(2.5f, 0, 27.5f); game_tick(TICK); }
        check("rampante : plus rapide que la course (s pour 16 m)", tick * TICK, 1, 16.0f / COURSE_V);
    }

    /* coeur et musique */
    test_reset(); test_isoler();
    { int b0 = coeurBat; test_run(120 * 10, -1, -1);
      check("coeur : au repos (battements en 10 s)", (float)(coeurBat - b0), 9, 14); }
    test_reset(); test_place(2.5f, 27.5f, PI / 2); test_monstre(MON_RODEUR, 9.5f, 27.5f);
    mon_etat(&mon[0], E_CHASSE, 0); mon[0].vitK = 0;     /* il te poursuit... sans avancer */
    { float b0 = bpm; test_run(120 * 4, -1, -1);
      check("coeur : poursuite -> le BPM grimpe", bpm - b0, 40, 130); }
    {
        double e = 0; float mx = 0, l, r;
        ambActif = 1; ambTension = 0.9f; ambTenL = 0.9f; ambEnv = 1;
        for (int i = 0; i < SR; i++) {
            if (i % 20000 == 0) amb_battement(1);
            amb_frame(&l, &r);
            e += l * l; mx = fmaxf(mx, fabsf(l));
        }
        check("musique : une seconde en poursuite (niveau RMS)", (float)sqrt(e / SR), 0.02f, 1.5f);
        check("musique : pas d'explosion (crete)", isfinite(mx) ? mx : 99, 0, 4);
    }

    /* objectif et ascenseur */
    test_reset(); test_isoler();
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

    /* le vrai niveau 1 et la geometrie */
    {
        carteTest = 0;
        reset_game(); cartonActif = 0;
        int libres, n = carte_atteignables(&libres), linteaux = 0, plinthes = 0, cloisons = 0;
        check("niveau 1 : toutes les cases libres atteignables", (float)(libres - n), 0, 0);
        check("niveau 1 : 3 fusibles, 1 machoire, l'ascenseur", (float)(nfus * 100 + npieges * 10 + asc.actif), 311, 311);
        check("niveau 1 : chambre, salle de bain, salle des cameras", (float)(npieces == 3 && pieces[0].zone == ZONE_CHAMBRE && pieces[1].zone == ZONE_SDB && pieces[2].zone == ZONE_CAMERAS), 1, 1);
        for (int i = 0; i < nfaces; i++) {
            if (faces[i].mat == MAT_PLINTHE) plinthes++;
            if (faces[i].mat == MAT_CLOISON) cloisons++;
            if (faces[i].mat == MAT_CORNICHE && faces[i].n.y < -0.5f && faces[i].o.y < LINTEAU_Y + 0.01f && faces[i].o.y > LINTEAU_Y - 0.01f) linteaux++;
        }
        check("geometrie : plinthes", (float)plinthes, 100, 99999);
        check("geometrie : linteaux au-dessus des passages", (float)linteaux, 4, 999);
        check("geometrie : cloisons basses", (float)cloisons, 10, 9999);
        check("geometrie : prises et grilles sur les murs", (float)ndecals, 10, MAXDECAL);
        check("cloison basse : on ne passe pas", (float)(cell_mur(11, 2) != 0), 1, 1);
        check("cloison basse : on voit par-dessus", (float)visible_xz(10.5f, 2.5f, 12.5f, 3.5f), 1, 1);
        check("niveau 1 : le monstre dort au debut", (float)monstresEveilles, 0, 0);
        carteTest = 1;
    }

    /* en ligne : instantane, et monstres face a plusieurs joueurs */
    {
        test_reset(); test_place(10.5f, 27.5f, PI / 2); test_monstre(MON_RODEUR, 22.5f, 27.5f);
        netMode = NET_HOTE; monSlot = 0;
        JP[1].actif = 1; snprintf(JP[1].nom, 16, "Ines");
        netj_de_P(&JP[1].st); JP[1].st.x = 35.5f; JP[1].st.z = 27.5f; JP[1].st.bruit = 100;   /* Ines court, a 13 m de lui */
        P.bruit = BRUIT_ACCROUPI; P.bruitImpulse = 0;
        monstres_tick(TICK, 0);
        check("multi : il poursuit celui qui fait du bruit (Ines)", (float)(mon[0].etat == E_CHASSE ? mon[0].proie : -1), 1, 1);
        mon[0].pos = v3(35.5f, 0, 27.4f);
        monstres_tick(TICK, 0);
        check("multi : il attrape Ines (annonce de l'hote)", (float)mortHote[1], MON_RODEUR + 1, MON_RODEUR + 1);
        Snap s; snap_faire(&s);
        unsigned char back[sizeof(Snap) + 8]; char b64[WS_LIGNE];
        int n = b64_enc((const unsigned char *)&s, sizeof s, b64), m = b64_dec(b64, back, sizeof back);
        check("multi : instantane en base64 (taille)", (float)n, 1, 2000);
        check("multi : instantane relu a l'identique", (m == (int)sizeof s && !memcmp(back, &s, sizeof s)) ? 1.0f : 0.0f, 1, 1);
        check("multi : l'instantane dit qu'Ines est morte", (float)(s.j[1].flags & JF_MORT ? 1 : 0), 1, 1);
        Snap c = s; c.fusMask = 1; c.piegeSeq[0] = 0;  /* cote client : un fusible pris par quelqu'un */
        netMode = NET_CLIENT; monSlot = 1;
        int avant = fusPris;
        snap_appliquer(&c);
        check("multi : le client voit le fusible pris", (float)(fusPris - avant), 1, 1);
        check("multi : le client voit le monstre", (float)nmon, 1, 1);
        netMode = NET_OFF; monSlot = 0; memset(JP, 0, sizeof JP); memset(mortHote, 0, sizeof mortHote);
    }

    /* pieces a theme, meubles, cachettes (la vitrine : un couloir et les six pieces) */
    {
        reset_game(); cartonActif = 0; monstres_reset();
        charger_vitrine(); monde_reconstruire(); player_reset(); nfus = 0;
        check("pieces : les sept pieces sont creusees", (float)npieces, 7, 7);
        int portesOk = 1;
        for (int i = 0; i < npieces; i++) {               /* une seule ouverture dans la bordure */
            const Piece *p = &pieces[i]; int ouv = 0;
            for (int z = p->z0 - 1; z <= p->z0 + p->ih; z++)
                for (int x = p->x0 - 1; x <= p->x0 + p->iw; x++) {
                    int bord = x == p->x0 - 1 || x == p->x0 + p->iw || z == p->z0 - 1 || z == p->z0 + p->ih;
                    if (bord && !cell_mur(x, z)) ouv++;
                }
            if (ouv != 1) portesOk = 0;
        }
        check("pieces : chacune a une seule porte", (float)portesOk, 1, 1);
        int libres, n = carte_atteignables(&libres);
        check("pieces : tout reste atteignable", (float)(libres - n), 0, 0);
        int fleuri = 0, carrelage = 0;
        for (int i = 0; i < nfaces; i++) { fleuri += faces[i].mat == MAT_FLEURI; carrelage += faces[i].mat == MAT_CARRELAGE; }
        check("pieces : papier fleuri et carrelage", (float)(fleuri > 0 && carrelage > 0), 1, 1);
        int lit = -1, arm = -1, lampe = -1, cas = -1, man = -1;
        for (int i = 0; i < nmeubles; i++) {
            if (meubles[i].type == MB_LIT && lit < 0) lit = i;
            if (meubles[i].type == MB_ARMOIRE && arm < 0) arm = i;
            if (meubles[i].type == MB_LAMPE && lampe < 0) lampe = i;
            if (meubles[i].type == MB_CASSETTE && cas < 0) cas = i;
            if (meubles[i].type == MB_MANNEQUIN && man < 0) man = i;
        }
        check("meubles : lit, armoire, lampe, cassette, mannequin", (float)(lit >= 0 && arm >= 0 && lampe >= 0 && cas >= 0 && man >= 0), 1, 1);
        {
            short cand[256]; float c[3], c2[3];
            V3 pl = v3(meubles[lampe].x, 0.6f, meubles[lampe].z);
            int nc = candidats(pl, cand, 256);
            lum_point(v3(pl.x, 0.01f, pl.z + 0.0f), v3(0, 1, 0), cand, nc, -1, c);
            check("lumiere : la lampe de chevet eclaire le sol (chaud)", c[0] - c[2], 0.15f, 9);
            meubles[lampe].lumiere >= 0 ? (void)0 : (void)0;
            neons[meubles[lampe].lumiere].etat = NEON_MORT;   /* eteinte : il fait noir */
            lum_point(v3(pl.x, 0.01f, pl.z), v3(0, 1, 0), cand, nc, -1, c2);
            check("lumiere : lampe eteinte, beaucoup moins", c2[0], 0, c[0] * 0.5f);
            neons[meubles[lampe].lumiere].etat = NEON_OK;
        }
        /* le lit bloque debout, on rampe dessous accroupi */
        {
            const Meuble *m = &meubles[lit];
            V3 av = view_dir(m->yaw, 0);                  /* on arrive par le pied du lit */
            test_place(m->x + av.x * 1.6f, m->z + av.z * 1.6f, m->yaw + PI);
            test_run(120 * 2, A_AVANCER, -1);
            check("meubles : debout, le lit arrete (m du centre)", dist_xz(P.pos, v3(m->x, 0, m->z)), 1.0f, 1.4f);
            test_run(120 * 3, A_AVANCER, A_ACCROUPI);
            check("cachette : accroupi, on passe sous le lit", (float)P.cache, 1, 1);
            check("cachette : sous le lit, la lampe s'eteint", (float)P.lampe, 0, 0);
            test_run(60, -1, -1);
            check("cachette : sous le lit, on reste a plat", (float)P.accroupi, 1, 1);
            test_monstre(MON_RODEUR, P.pos.x + 0.2f, P.pos.z);   /* il est juste la... */
            mon_etat(&mon[0], E_CHASSE, 0); mon[0].vitK = 0;
            test_run(120 * 2, -1, -1);
            check("cachette : sous le lit, il ne t'attrape pas", (float)P.dead, 0, 0);
            monstres_reset();
        }
        /* l'armoire : E pour entrer, E pour sortir */
        {
            const Meuble *m = &meubles[arm];
            V3 av = view_dir(m->yaw, 0);
            test_place(m->x + av.x * 0.95f, m->z + av.z * 0.95f, m->yaw + PI);
            botPress[A_UTILISER] = 1; test_run(1, -1, -1);
            check("armoire : E -> cache dedans", (float)P.cache, 2, 2);
            test_run(120, A_AVANCER, -1);
            check("armoire : dedans, on ne bouge pas", dist_xz(P.pos, v3(m->x, 0, m->z)), 0, 0.01f);
            test_monstre(MON_RODEUR, m->x + av.x * 0.5f, m->z + av.z * 0.5f);
            mon_etat(&mon[0], E_CHASSE, 0); mon[0].vitK = 0;
            test_run(120, -1, -1);
            check("armoire : il ne t'y trouve pas", (float)P.dead, 0, 0);
            monstres_reset();
            botPress[A_UTILISER] = 1; test_run(2, -1, -1);
            check("armoire : E -> on ressort devant", (float)(P.cache == 0 && dist_xz(P.pos, v3(m->x, 0, m->z)) > 0.6f), 1, 1);
        }
        /* les mannequins : ils ne bougent pas sous les yeux */
        {
            Meuble *m = &meubles[man];
            V3 q = v3(m->x, 0, m->z);
            int ip = m->piece;
            pieces[ip].vue = 1;
            V3 porte = v3(pieces[ip].porteX + 0.5f, 0, pieces[ip].porteZ + 0.5f);
            test_place(porte.x, porte.z, atan2f(q.x - porte.x, -(q.z - porte.z)));   /* on le fixe depuis la porte */
            int e0 = m->etat; float x0 = m->x;
            P.lampe = 1;
            test_run(120 * 6, -1, -1);
            check("mannequins : regardes, ils ne bougent pas", (float)(m->etat == e0 && m->x == x0), 1, 1);
            P.yaw += PI;                                  /* on se retourne... */
            int bouge = 0;
            for (int i = 0; i < 120 * 15 && !bouge; i++) { test_run(1, -1, -1); P.yaw = P.lampeYaw = atan2f(q.x - porte.x, -(q.z - porte.z)) + PI; bouge = m->etat != e0; }
            check("mannequins : dos tourne, ils bougent", (float)bouge, 1, 1);
        }
        /* la cassette de Noe */
        {
            const Meuble *m = &meubles[cas];
            test_place(m->x, m->z + 0.6f, 0);
            P.yaw = atan2f(m->x - P.pos.x, -(m->z - P.pos.z));
            botPress[A_UTILISER] = 1; test_run(1, -1, -1);
            check("cassette : ramassee", (float)meubles[cas].etat, 1, 1);
            check("cassette : on la regarde (texte)", (float)cartonActif, 1, 1);
            cartonActif = 0;
        }
        /* en ligne : l'etat des meubles voyage dans l'instantane */
        {
            netMode = NET_HOTE;
            Snap s; snap_faire(&s);
            int tv = -1; for (int i = 0; i < nmeubles; i++) if (meubles[i].type == MB_TELE) tv = i;
            int avant = meubles[tv].etat;
            meubles[tv].etat = 9; meubles[cas].etat = 0;   /* le client a un autre etat... */
            netMode = NET_CLIENT; monSlot = 1;
            snap_appliquer(&s);
            check("multi : la tele reprend l'etat de l'hote", (float)meubles[tv].etat, (float)avant, (float)avant);
            check("multi : la cassette prise chez l'hote", (float)meubles[cas].etat, 1, 1);
            netMode = NET_OFF; monSlot = 0; cartonActif = 0; msgT = 0;
        }
        /* les niveaux generes ont leurs pieces, leurs meubles de bureau, leurs cameras */
        carteTest = 0; niveau_charger_ex(3, 0); carteTest = 1;
        check("pieces : niveau 3 genere, campement et salle des cameras", (float)npieces, 4, 5);
        check("niveaux : plus petits (cote du niveau 3)", (float)carteW, 25, CARTE_GEN_MIN + 3);
        {
            int bureaux = 0, cams = 0;
            for (int i = 0; i < nmeubles; i++) { bureaux += meubles[i].type == MB_BUREAU || meubles[i].type == MB_CLASSEUR || meubles[i].type == MB_FONTAINE || meubles[i].type == MB_PLANTE; cams += meubles[i].type == MB_CAMERA; }
            check("bureaux : des meubles dans les zones ordinaires", (float)bureaux, 3, 999);
            check("cameras : posees dans le niveau", (float)(ncameras >= 2 && cams == ncameras), 1, 1);
            int libres, n = carte_atteignables(&libres);
            check("bureaux : les meubles ne coupent aucun passage", (float)(libres - n), 0, 0);
        }
        {                                                 /* mini-carte : marcher decouvre */
            int avant = 0, apres = 0;
            for (int z = 0; z < carteH; z++) for (int x = 0; x < carteW; x++) avant += decouvert[z][x];
            test_isoler(); test_run(120 * 3, A_AVANCER, -1);
            for (int z = 0; z < carteH; z++) for (int x = 0; x < carteW; x++) apres += decouvert[z][x];
            check("mini-carte : marcher decouvre des cases", (float)(apres - avant), 5, 9999);
        }
    }

    /* micro : chuchoter passe, crier attire */
    micGain = MICRO_GAIN_DEFAUT;
    micNiveau = 0.015f; check("micro : un chuchotement ne fait aucun bruit", micro_bruit(), 0, 0);
    micNiveau = 0.08f; check("micro : parler normalement s'entend", micro_bruit(), 0.4f, 1.2f);
    micNiveau = 0.35f; check("micro : un cri = bruit maximal", micro_bruit(), MICRO_MAX, MICRO_MAX);
    test_reset(); test_place(10.5f, 27.5f, PI / 2); test_monstre(MON_RODEUR, 24.5f, 27.5f);
    micOuvert = 1; micNiveau = 0.35f;                     /* on simule un cri dans le micro, immobile */
    test_run(2, -1, -1);
    micOuvert = 0; micNiveau = 0;
    check("micro : crier attire le Rodeur a 14 m", (float)mon[0].etat, E_CHASSE, E_CHASSE);

    /* mise a jour et reglages */
    {
        UpdInfo in;
        int ok = upd_parse("build=42\nsha=abc1234\nwin_size=1000\nwin_sha256=AB\nmac_size=7\nlinux_size=9\n", &in);
        check("mise a jour : version.txt lu (build)", (float)(ok ? in.build : -1), 42, 42);
        check("mise a jour : tailles lues", (float)(in.winSize + in.macSize + in.linSize), 1016, 1016);
        Sha256 sh; char hex[65];
        sha_init(&sh); sha_update(&sh, "abc", 3); sha_final(&sh, hex);
        check("mise a jour : sha256(\"abc\")", strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0 ? 1.0f : 0.0f, 1, 1);
        check("mise a jour : une version plus recente est proposee", (float)(in.build > BOURDON_BUILD), 1, 1);
    }
    {
        char garde[1024]; snprintf(garde, sizeof garde, "%s", cfgPath);
        snprintf(cfgPath, sizeof cfgPath, "%s.cfg", path);
        masterVol = 0.7f; micOn = 1; micGain = 6.5f; flashsOn = 0; sens = 0.003f;
        cfg_save();
        masterVol = 1; micOn = 0; micGain = 1; flashsOn = 1; sens = 0.001f;
        cfg_load();
        check("reglages : sauvegardes puis relus", (fabsf(masterVol - 0.7f) < 0.01f && micOn && fabsf(micGain - 6.5f) < 0.01f && !flashsOn
                                                   && fabsf(sens - 0.003f) < 0.0001f) ? 1.0f : 0.0f, 1, 1);
        remove(cfgPath);
        snprintf(cfgPath, sizeof cfgPath, "%s", garde);
        micOn = 0; flashsOn = 1;
    }

    fprintf(testOut, "\n%d tests, %d echec(s)\n", testCount, testFails);
    fclose(testOut);
    return testFails ? 1 : 0;
}
