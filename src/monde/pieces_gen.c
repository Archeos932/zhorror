/* BOURDONNEMENT - pieces a theme : creusees dans les murs, une seule porte, meublees selon un gabarit
   Morceau de main.c : inclus dans l'ordre (apres monde/carte.c, avant monde/generation.c), ne se compile pas seul.

   placer_piece(zone) cherche un bloc de murs plein (interieur 4x4 a 6x5 + bordure) qui touche une case libre deja
   accessible, creuse l'interieur et une porte (le linteau automatique de rendu/eclairage.c en fait une vraie porte),
   puis meubler() pose les meubles. Le repere d'une piece : le "fond" est le mur oppose a la porte ; piece_pt(a, b)
   donne un point a b metres du fond, decale de a metres le long du fond (a > 0 : a droite en entrant). */

static int irand_p(int n) { return n > 0 ? (int)(frand() * n) % n : 0; }

/* vecteurs du repere : f vers le fond, l lateral (droite en entrant) ; demi-largeur et profondeur */
static void piece_repere(const Piece *p, V3 *f, V3 *l, float *demiL, float *prof) {
    *f = v3((float)cotes[p->coteFond][0], 0, (float)cotes[p->coteFond][1]);   /* vers le fond (oppose a la porte) */
    *l = v3(-f->z, 0, f->x);
    int lateralX = fabsf(f->z) > 0.5f;                    /* fond horizontal (z) : le lateral est x */
    *demiL = (lateralX ? p->iw : p->ih) * 0.5f;
    *prof = lateralX ? (float)p->ih : (float)p->iw;
}
static V3 piece_pt(const Piece *p, float a, float b) {
    V3 f, l; float dl, pr;
    piece_repere(p, &f, &l, &dl, &pr);
    V3 c = v3(p->x0 + p->iw * 0.5f, 0, p->z0 + p->ih * 0.5f);
    V3 fond = vadd(c, vmul(f, pr * 0.5f));
    return vadd(vadd(fond, vmul(f, -b)), vmul(l, a));
}
static float yaw_vers(V3 d) { return atan2f(d.x, -d.z); }    /* le devant du meuble regarde dans la direction d */

/* meuble pose dans le repere de la piece, son devant tourne vers dir */
static int poser(int type, const Piece *p, int ip, float a, float b, V3 dir) {
    V3 q = piece_pt(p, a, b);
    return add_meuble(type, q.x, q.z, yaw_vers(dir), ip);
}

static int add_lampe(float x, float z, float y, int etat) {   /* une lumiere de meuble (lampe de chevet, lanterne) */
    add_neon((int)floorf(x), (int)floorf(z), etat);
    Neon *n = &neons[nneons - 1];
    n->x = x; n->z = z; n->y = y; n->lampe = 1;
    return nneons - 1;
}

static void meubler(int ip) {
    Piece *p = &pieces[ip];
    V3 f, l; float dl, pr;
    piece_repere(p, &f, &l, &dl, &pr);
    V3 versPorte = vmul(f, -1);
    int cote = frand() < 0.5f ? 1 : -1;                   /* le cote de l'armoire, du frigo... */
    V3 c = v3(p->x0 + p->iw * 0.5f, 0, p->z0 + p->ih * 0.5f);
    switch (p->zone) {
    case ZONE_CHAMBRE: {
        { int li = poser(MB_LIT, p, ip, 0, 1.05f, versPorte); if (li >= 0 && frand() < 0.5f) meubles[li].etat = 1; }   /* une forme sous les draps */
        poser(MB_CHEVET, p, ip, 1.0f, 0.25f, versPorte);
        poser(MB_CHEVET, p, ip, -1.0f, 0.25f, versPorte);
        int la = poser(MB_LAMPE, p, ip, cote * 1.0f, 0.25f, versPorte);
        if (la >= 0) meubles[la].lumiere = add_lampe(meubles[la].x, meubles[la].z, 0.85f, NEON_OK);
        poser(MB_ARMOIRE, p, ip, cote * (dl - 0.32f), 2.5f, vmul(l, (float)-cote));
        int tv = poser(MB_TELE, p, ip, -cote * (dl - 0.27f), 1.4f, vmul(l, (float)cote));
        if (tv >= 0) meubles[tv].etat = 1;                /* la neige */
        break;
    }
    case ZONE_SDB:
        { int b = poser(MB_BAIGNOIRE, p, ip, cote * (dl - 0.42f), 0.9f, versPorte); if (b >= 0) meubles[b].px[0] = (float)cote; }   /* rideau cote piece */
        poser(MB_LAVABO, p, ip, -cote * 0.6f, 0.23f, versPorte);
        poser(MB_TOILETTES, p, ip, -cote * (dl - 0.32f), 1.6f, vmul(l, (float)cote));
        add_neon((int)floorf(c.x), (int)floorf(c.z), NEON_FOU);   /* neon blafard qui sursaute */
        break;
    case ZONE_ENFANT: {
        poser(MB_PETIT_LIT, p, ip, -cote * 0.75f, 0.75f, versPorte);
        poser(MB_CHEVET, p, ip, cote * 0.55f, 0.25f, versPorte);
        int bm = poser(MB_BOITE_MUSIQUE, p, ip, cote * 0.55f, 0.25f, versPorte);
        if (bm >= 0) meubles[bm].lumiere = add_lampe(meubles[bm].x, meubles[bm].z, 0.7f, NEON_OK);   /* veilleuse */
        poser(MB_JOUETS, p, ip, 0.3f, pr * 0.55f, versPorte);
        poser(MB_JOUETS, p, ip, -0.6f, pr * 0.7f, l);
        poser(MB_ARMOIRE, p, ip, cote * (dl - 0.32f), 2.4f, vmul(l, (float)-cote));
        break;
    }
    case ZONE_CUISINE:
        poser(MB_COMPTOIR, p, ip, -0.55f, 0.32f, versPorte);
        poser(MB_COMPTOIR, p, ip, 0.55f, 0.32f, versPorte);
        poser(MB_FRIGO, p, ip, cote * (dl - 0.36f), 1.1f, vmul(l, (float)-cote));
        { int tb = poser(MB_TABLE, p, ip, -cote * 0.3f, pr * 0.55f, l); if (tb >= 0) meubles[tb].etat = 1; }   /* renversee */
        poser(MB_CHAISE, p, ip, -cote * 1.1f, pr * 0.5f, l);
        poser(MB_CHAISE, p, ip, cote * 0.5f, pr * 0.7f, vmul(f, 1));
        add_neon((int)floorf(c.x), (int)floorf(c.z), NEON_FOU);
        break;
    case ZONE_RESERVE: {
        for (int r = 0; r < 2; r++)
            for (int s = -1; s <= 1; s += 2) poser(MB_RAYONNAGE, p, ip, s * 0.55f, 0.3f + r * 1.5f, versPorte);
        poser(MB_CARTONS, p, ip, cote * (dl - 0.35f), pr - 1.2f, versPorte);
        int nm = 3 + irand_p(3);
        for (int k = 0; k < nm; k++) {                    /* mannequins : trois places possibles chacun */
            int im = add_meuble(MB_MANNEQUIN, 0, 0, 0, ip);
            if (im < 0) break;
            Meuble *m = &meubles[im];
            for (int j = 0; j < 3; j++) {
                float a = (frand() * 2 - 1) * (dl - 0.4f), b = 0.9f + frand() * (pr - 2.0f);
                if (fabsf(b - 0.3f) < 0.45f || fabsf(b - 1.8f) < 0.45f) b += 0.7f;   /* pas dans les rayonnages */
                V3 q = piece_pt(p, a, fminf(b, pr - 0.9f));
                m->px[j] = q.x; m->pz[j] = q.z; m->pyaw[j] = frand() * 2 * PI;
            }
            m->x = m->px[0]; m->z = m->pz[0]; m->yaw = m->pyaw[0];
        }
        break;
    }
    case ZONE_NOE: {
        poser(MB_SAC_COUCHAGE, p, ip, -0.6f, 1.0f, versPorte);
        int ln = poser(MB_LANTERNE, p, ip, 0.25f, 0.6f, versPorte);
        if (ln >= 0) meubles[ln].lumiere = add_lampe(meubles[ln].x, meubles[ln].z, 0.4f, NEON_FOU);   /* elle vacille */
        poser(MB_CASSETTE, p, ip, 0.45f, 1.25f, versPorte);
        poser(MB_CARTONS, p, ip, cote * (dl - 0.35f), 0.35f, versPorte);
        poser(MB_CARTONS, p, ip, cote * (dl - 0.35f), 1.0f, versPorte);
        break;
    }
    }
}

/* Seconde facon : la piece est construite par-dessus une zone ouverte (bordure muree, une porte, interieur creuse),
   puis on verifie que tout le niveau reste atteignable ; sinon tout est remis comme avant. */
static int placer_piece_construite(int zt) {
    static unsigned char g0[CARTE_MAX][CARTE_MAX], s0[CARTE_MAX][CARTE_MAX];
    int sx = (int)carteDepart.x, sz = (int)carteDepart.z;
    for (int essai = 0; essai < 300; essai++) {
        int iw = 4 + irand_p(3), ih = 4 + irand_p(2);
        int x0 = 2 + irand_p(carteW - iw - 4), z0 = 2 + irand_p(carteH - ih - 4), ok = 1;
        for (int z = z0 - 1; z <= z0 + ih && ok; z++)    /* rien d'important dessous */
            for (int x = x0 - 1; x <= x0 + iw && ok; x++) {
                if (zone[z][x] || occupe[z][x] || (abs(x - sx) <= 1 && abs(z - sz) <= 1)) ok = 0;
                if (asc.actif && abs(x - asc.gx) <= 1 && abs(z - asc.gz) <= 1) ok = 0;
                for (int f = 0; f < nfus; f++) if ((int)fus[f].pos.x == x && (int)fus[f].pos.z == z) ok = 0;
            }
        if (!ok) continue;
        int cand[64][3], nc = 0;                          /* porte : une case de bordure qui donne sur une case libre */
        for (int z = z0; z < z0 + ih; z++)
            for (int k = 0; k < 2; k++) {
                int bx = k ? x0 + iw : x0 - 1, ox = k ? bx + 1 : bx - 1;
                if (!cell_mur(ox, z) && !zone[z][ox] && nc < 64) { cand[nc][0] = bx; cand[nc][1] = z; cand[nc][2] = k ? 0 : 1; nc++; }
            }
        for (int x = x0; x < x0 + iw; x++)
            for (int k = 0; k < 2; k++) {
                int bz = k ? z0 + ih : z0 - 1, oz = k ? bz + 1 : bz - 1;
                if (!cell_mur(x, oz) && !zone[oz][x] && nc < 64) { cand[nc][0] = x; cand[nc][1] = bz; cand[nc][2] = k ? 2 : 3; nc++; }
            }
        if (!nc) continue;
        memcpy(g0, grille, sizeof g0); memcpy(s0, sansNeon, sizeof s0);
        int c = irand_p(nc);
        for (int z = z0 - 1; z <= z0 + ih; z++)
            for (int x = x0 - 1; x <= x0 + iw; x++) {
                int bord = x == x0 - 1 || x == x0 + iw || z == z0 - 1 || z == z0 + ih;
                grille[z][x] = bord ? 1 : 0;
                if (!bord) { sansNeon[z][x] = 1; sol[z][x] = SOL_NORMAL; }
            }
        grille[cand[c][1]][cand[c][0]] = 0; sansNeon[cand[c][1]][cand[c][0]] = 1;
        int libres, n = carte_atteignables(&libres);
        if (n != libres) { memcpy(grille, g0, sizeof g0); memcpy(sansNeon, s0, sizeof s0); continue; }   /* il coupait le niveau */
        for (int z = z0; z < z0 + ih; z++) for (int x = x0; x < x0 + iw; x++) zone[z][x] = (unsigned char)zt;
        Piece *p = &pieces[npieces];
        memset(p, 0, sizeof *p);
        p->zone = zt; p->x0 = x0; p->z0 = z0; p->iw = iw; p->ih = ih; p->porteX = cand[c][0]; p->porteZ = cand[c][1];
        p->coteFond = cand[c][2] ^ 1;
        npieces++;
        meubler(npieces - 1);
        return npieces - 1;
    }
    return -1;
}

/* creuse une piece de la zone donnee ; renvoie son indice ou -1. distCase doit etre a jour (cases accessibles). */
static int placer_piece(int zt) {
    if (npieces >= MAXPIECES) return -1;
    for (int essai = 0; essai < 500; essai++) {
        int iw = 4 + irand_p(3), ih = 4 + irand_p(2);
        int x0 = 2 + irand_p(carteW - iw - 4), z0 = 2 + irand_p(carteH - ih - 4);
        int plein = 1;                                    /* bordure comprise : que des murs pleins */
        for (int z = z0 - 1; z <= z0 + ih && plein; z++)
            for (int x = x0 - 1; x <= x0 + iw && plein; x++)
                if (cell_mur(x, z) != 1 || (asc.actif && x == asc.gx && z == asc.gz)) plein = 0;
        if (!plein) continue;
        int cand[64][3], nc = 0;                          /* portes possibles : bordure, case dehors accessible */
        for (int z = z0; z < z0 + ih; z++)
            for (int k = 0; k < 2; k++) {
                int bx = k ? x0 + iw : x0 - 1, ox = k ? bx + 1 : bx - 1;
                if (!cell_mur(ox, z) && distCase[z][ox] >= 0 && !zone[z][ox] && nc < 64) { cand[nc][0] = bx; cand[nc][1] = z; cand[nc][2] = k ? 0 : 1; nc++; }
            }
        for (int x = x0; x < x0 + iw; x++)
            for (int k = 0; k < 2; k++) {
                int bz = k ? z0 + ih : z0 - 1, oz = k ? bz + 1 : bz - 1;
                if (!cell_mur(x, oz) && distCase[oz][x] >= 0 && !zone[oz][x] && nc < 64) { cand[nc][0] = x; cand[nc][1] = bz; cand[nc][2] = k ? 2 : 3; nc++; }
            }
        if (!nc) continue;
        int c = irand_p(nc);
        for (int z = z0; z < z0 + ih; z++)
            for (int x = x0; x < x0 + iw; x++) { grille[z][x] = 0; zone[z][x] = (unsigned char)zt; sansNeon[z][x] = 1; sol[z][x] = SOL_NORMAL; }
        grille[cand[c][1]][cand[c][0]] = 0;
        sansNeon[cand[c][1]][cand[c][0]] = 1;
        Piece *p = &pieces[npieces];
        memset(p, 0, sizeof *p);
        p->zone = zt; p->x0 = x0; p->z0 = z0; p->iw = iw; p->ih = ih; p->porteX = cand[c][0]; p->porteZ = cand[c][1];
        p->coteFond = cand[c][2] ^ 1;                     /* cotes[] : +x/-x, +z/-z vont par paires ; le fond est a l'oppose */
        npieces++;
        meubler(npieces - 1);
        return npieces - 1;
    }
    return placer_piece_construite(zt);                   /* pas de bloc de murs libre : on la construit */
}

/* plusieurs pieces ; la carte des distances est recalculee depuis le depart */
static void placer_pieces(const int *zones, int n) {
    carte_distances((int)carteDepart.x, (int)carteDepart.z);
    for (int i = 0; i < n; i++) {
        placer_piece(zones[i]);
        carte_distances((int)carteDepart.x, (int)carteDepart.z);
    }
}
