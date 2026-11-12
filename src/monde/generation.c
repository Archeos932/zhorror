/* BOURDONNEMENT - generation d'un niveau par graine (meme graine = meme niveau, chez tous les joueurs plus tard)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).

   Salles qui se chevauchent, couloirs en L, halls a piliers, bouts de murs au milieu des grandes salles (le
   labyrinthe de bureaux), zones noires. Puis : cases isolees rebouchees, cabine d'ascenseur creusee dans un mur au
   bout le plus lointain, fusibles disperses loin du depart et loin les uns des autres. */

static int genEau = 3, genVerre = 3, genMachoires = 2, genAlarmes = 0;   /* reglees par le niveau (niveaux.c) */
static int genZones[6], genNZones;                       /* pieces a theme a creuser (niveaux.c) */

static int irand(int n) { return n > 0 ? (int)(frand() * n) % n : 0; }

static void creuser(int x0, int z0, int x1, int z1) {    /* rectangle inclus, en gardant le bord de la carte */
    for (int z = z0; z <= z1; z++)
        for (int x = x0; x <= x1; x++)
            if (x > 0 && z > 0 && x < carteW - 1 && z < carteH - 1) grille[z][x] = 0;
}

static void generer_niveau(unsigned graine, int w, int h) {
    unsigned keep = rng;
    rng = graine * 2654435761u + 12345u;
    carte_vider(w, h);
    int cx[64], cz[64], nr = 0, nsalles = w * h / 70 + 4;
    if (nsalles > 64) nsalles = 64;
    for (int i = 0; i < nsalles; i++) {                   /* salles */
        int rw = 3 + irand(8), rh = 3 + irand(8);
        int x = 1 + irand(w - rw - 2), z = 1 + irand(h - rh - 2);
        creuser(x, z, x + rw - 1, z + rh - 1);
        cx[nr] = x + rw / 2; cz[nr] = z + rh / 2; nr++;
        if (rw >= 6 && rh >= 6 && frand() < 0.45f)        /* hall a piliers */
            for (int pz = z + 2; pz < z + rh - 2; pz += 2)
                for (int px = x + 2; px < x + rw - 2; px += 2) grille[pz][px] = 1;
    }
    for (int i = 1; i < nr; i++) {                        /* couloirs en L (largeur 1 ou 2) entre salles successives */
        int a = i - 1, larg = frand() < 0.35f ? 1 : 0;
        int x0 = cx[a], z0 = cz[a], x1 = cx[i], z1 = cz[i];
        if (frand() < 0.5f) {
            creuser(x0 < x1 ? x0 : x1, z0, x0 < x1 ? x1 : x0, z0 + larg);
            creuser(x1, z0 < z1 ? z0 : z1, x1 + larg, z0 < z1 ? z1 : z0);
        } else {
            creuser(x0, z0 < z1 ? z0 : z1, x0 + larg, z0 < z1 ? z1 : z0);
            creuser(x0 < x1 ? x0 : x1, z1, x0 < x1 ? x1 : x0, z1 + larg);
        }
    }
    int sx = cx[0], sz = cz[0];                           /* depart : centre de la premiere salle, degage */
    for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) grille[sz + dz][sx + dx] = 0;
    carteDepart = v3(sx + 0.5f, 0, sz + 0.5f);
    carte_distances(sx, sz);                              /* cases isolees : rebouchees */
    int libres = 0;
    for (int z = 0; z < carteH; z++)
        for (int x = 0; x < carteW; x++) {
            if (!grille[z][x] && distCase[z][x] < 0) grille[z][x] = 1;
            libres += !grille[z][x];
        }
    for (int k = 0; k < w * h / 45; k++) {                /* bouts de murs (cloisons de bureaux), sans jamais couper */
        int x = 2 + irand(w - 4), z = 2 + irand(h - 4), dir = irand(2), len = 2 + irand(4), pose[6][2], np = 0;
        int typeMur = frand() < 0.45f ? 2 : 1;            /* pres d'une fois sur deux : cloison basse de bureau */
        for (int s = 0; s < len; s++) {
            int px = x + (dir ? s : 0), pz = z + (dir ? 0 : s);
            if (px >= carteW - 1 || pz >= carteH - 1 || grille[pz][px] || abs(px - sx) + abs(pz - sz) <= 2) continue;
            grille[pz][px] = (unsigned char)typeMur; pose[np][0] = px; pose[np][1] = pz; np++;
        }
        if (!np) continue;
        int n = 0;
        carte_distances(sx, sz);
        for (int zz = 0; zz < carteH; zz++) for (int xx = 0; xx < carteW; xx++) n += distCase[zz][xx] >= 0;
        if (n == libres - np) libres -= np;
        else for (int i = 0; i < np; i++) grille[pose[i][1]][pose[i][0]] = 0;   /* il coupait le niveau : retire */
    }
    placer_pieces(genZones, genNZones);                   /* chambres, salles de bain... (recalcule distCase) */
    carte_distances(sx, sz);

    for (int k = 0; k < 2 + irand(3); k++) {              /* zones noires (sans neon) */
        int rw = 4 + irand(8), rh = 4 + irand(8), x = irand(w - rw), z = irand(h - rh);
        for (int zz = z; zz < z + rh; zz++) for (int xx = x; xx < x + rw; xx++) sansNeon[zz][xx] = 1;
    }

    {                                                     /* ascenseur : mur avec une seule case libre voisine, au plus loin */
        int best = -1, bx = 0, bz = 0;
        for (int z = 1; z < carteH - 1; z++)
            for (int x = 1; x < carteW - 1; x++) {
                if (grille[z][x] != 1) continue;
                int libres = 0, d = -1;
                for (int k = 0; k < 4; k++) {
                    int nx = x + cotes[k][0], nz = z + cotes[k][1];
                    if (!cell_mur(nx, nz)) { libres++; d = distCase[nz][nx]; }
                }
                if (libres == 1 && d > best) { best = d; bx = x; bz = z; }
            }
        if (best >= 0) { grille[bz][bx] = 0; poser_ascenseur(bx, bz); }
    }
    {                                                     /* fusibles : loin du depart, loin les uns des autres, contre un mur */
        for (int f = 0; f < FUSIBLES_N; f++) {
            float bestS = -1; int bx = -1, bz = -1;
            for (int z = 1; z < carteH - 1; z++)
                for (int x = 1; x < carteW - 1; x++) {
                    if (grille[z][x] || distCase[z][x] < 8) continue;
                    if (asc.actif && abs(x - asc.gx) + abs(z - asc.gz) < 3) continue;
                    int murs = 0;
                    for (int k = 0; k < 4; k++) murs += cell_mur(x + cotes[k][0], z + cotes[k][1]);
                    if (!murs) continue;
                    float dmin = 1e9f;
                    for (int j = 0; j < nfus; j++) {
                        float dx = fus[j].pos.x - (x + 0.5f), dz = fus[j].pos.z - (z + 0.5f);
                        dmin = fminf(dmin, sqrtf(dx * dx + dz * dz));
                    }
                    float s = fminf((float)distCase[z][x], 40.0f) * 0.5f + fminf(dmin, 30.0f) + frand() * 6.0f;
                    if (s > bestS) { bestS = s; bx = x; bz = z; }
                }
            if (bx >= 0) add_fusible(bx, bz);
        }
    }
    for (int k = 0; k < genEau; k++) {                    /* flaques : taches rondes et irregulieres */
        int x = 1 + irand(carteW - 2), z = 1 + irand(carteH - 2), r = 1 + irand(3);
        if (grille[z][x] || distCase[z][x] < 4) continue;
        for (int dz = -r; dz <= r; dz++)
            for (int dx = -r; dx <= r; dx++)
                if (dx * dx + dz * dz <= r * r + irand(3) && !cell_mur(x + dx, z + dz)) sol[z + dz][x + dx] = SOL_EAU;
    }
    for (int k = 0; k < genVerre; k++) {                  /* verre brise : petites plaques */
        int x = 1 + irand(carteW - 2), z = 1 + irand(carteH - 2);
        if (grille[z][x] || distCase[z][x] < 4) continue;
        for (int j = 0; j < 2 + irand(3); j++) {
            int px = x + irand(3) - 1, pz = z + irand(3) - 1;
            if (!cell_mur(px, pz)) sol[pz][px] = SOL_VERRE;
        }
    }
    for (int k = 0, essais = 0; k < genMachoires && essais < 2000; essais++) {   /* machoires : dans les couloirs */
        int x = 1 + irand(carteW - 2), z = 1 + irand(carteH - 2);
        if (grille[z][x] || distCase[z][x] < 6) continue;
        int ex = !cell_mur(x - 1, z) && !cell_mur(x + 1, z) && cell_mur(x, z - 1) && cell_mur(x, z + 1);
        int ez = !cell_mur(x, z - 1) && !cell_mur(x, z + 1) && cell_mur(x - 1, z) && cell_mur(x + 1, z);
        if (!ex && !ez) continue;
        int libre = 1;
        for (int i = 0; i < nfus; i++) if ((int)fus[i].pos.x == x && (int)fus[i].pos.z == z) libre = 0;
        for (int i = 0; i < npieges; i++) if (fabsf(pieges[i].pos.x - (x + 0.5f)) + fabsf(pieges[i].pos.z - (z + 0.5f)) < 6) libre = 0;
        if (asc.actif && abs(x - asc.gx) + abs(z - asc.gz) < 4) libre = 0;
        if (!libre) continue;
        add_piege(x, z, PIEGE_MACHOIRE); k++;
    }
    for (int k = 0, essais = 0; k < genAlarmes && essais < 2000; essais++) {    /* faux fusibles : contre un mur, loin */
        int x = 1 + irand(carteW - 2), z = 1 + irand(carteH - 2);
        if (grille[z][x] || distCase[z][x] < 10) continue;
        int murs = 0;
        for (int j = 0; j < 4; j++) murs += cell_mur(x + cotes[j][0], z + cotes[j][1]);
        if (!murs) continue;
        add_piege(x, z, PIEGE_ALARME); k++;
    }
    placer_neons();
    {                                                     /* le joueur regarde vers la plus longue ouverture */
        float bestL = -1;
        for (int k = 0; k < 4; k++) {
            float l = ray_grille(carteDepart.x, carteDepart.z, (float)cotes[k][0], (float)cotes[k][1], 40);
            if (l > bestL) { bestL = l; carteDepartYaw = atan2f((float)cotes[k][0], -(float)cotes[k][1]); }
        }
    }
    rng = keep;
}
