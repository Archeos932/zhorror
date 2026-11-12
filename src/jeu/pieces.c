/* BOURDONNEMENT - la vie des pieces a theme : un evenement au premier passage, les mannequins qui bougent quand
   personne ne les regarde, les sons d'ambiance (tele, frigo) en 3D, la cassette de Noe.
   Morceau de main.c : inclus dans l'ordre (apres jeu/screamers.c), ne se compile pas seul.

   En ligne, l'hote decide (pieces_tick) et l'etat des meubles "vivants" voyage dans l'instantane (reseau.c) ;
   chez tout le monde, meuble_etat() joue les sons d'un changement d'etat. */

static int voixBoucle[2], srcBoucle[2] = { -1, -1 };      /* deux boucles 3D (tele, frigo) attribuees aux plus proches */
static float boucleChoixT, goutteT;

/* les meubles dont l'etat change en jeu (dans cet ordre, identique chez tous : la generation est la meme) */
static int meuble_vivant(int type) {
    return type == MB_TELE || type == MB_FRIGO || type == MB_BAIGNOIRE || type == MB_LIT || type == MB_MANNEQUIN ||
           type == MB_CASSETTE || type == MB_BOITE_MUSIQUE || type == MB_ARMOIRE;
}

static int piece_ou(float x, float z) {
    for (int i = 0; i < npieces; i++) {
        const Piece *p = &pieces[i];
        if (x >= p->x0 && x < p->x0 + p->iw && z >= p->z0 && z < p->z0 + p->ih) return i;
    }
    return -1;
}

/* un mannequin a sa place k (0..2), tourne vers la direction d (0..7) */
static void mannequin_placer(Meuble *m, int etat) {
    int k = (etat / 8) % 3, d = etat % 8;
    m->x = m->px[k]; m->z = m->pz[k]; m->yaw = d * PI / 4;
}

/* change l'etat d'un meuble et joue ce qui va avec (hote et clients) */
static void meuble_etat(int i, int e) {
    Meuble *m = &meubles[i];
    if (m->etat == e) return;
    int avant = m->etat;
    m->etat = e; m->t = 0;
    V3 p = v3(m->x, 1, m->z);
    int ici = piece_ou(P.pos.x, P.pos.z) == m->piece;     /* le joueur local est dans la piece */
    switch (m->type) {
    case MB_TELE:
        if (e == 0) play3d(S_LAMPE, p, 0.7f, 0.6f, 10);   /* elle s'eteint d'un coup */
        if (e == 2 && ici) { play(S_STINGER, 0.9f, 1.2f); sursauter(BPM_SURSAUT); }
        break;
    case MB_FRIGO: if (avant == 0) play3d(S_GRINCEMENT, p, 0.9f, 0.8f, 12); break;
    case MB_ARMOIRE: play3d(S_GRINCEMENT, p, 0.8f, 1.0f + 0.1f * frand(), 10); break;
    case MB_BAIGNOIRE: if (e == 0 && avant == 1 && ici) { play(S_STINGER, 0.6f, 1.5f); sursauter(BPM_SURSAUT * 0.7f); play3d(S_GOUTTE, p, 1.0f, 0.7f, 8); } break;
    case MB_LIT: if (e == 0 && avant == 1 && ici) { play(S_STINGER, 0.5f, 1.4f); sursauter(BPM_SURSAUT * 0.6f); } break;
    case MB_BOITE_MUSIQUE: if (e == 1) play3d(S_BOITE, p, 0.9f, 1.0f, 14); break;
    case MB_MANNEQUIN: mannequin_placer(m, e); break;
    case MB_CASSETTE:
        if (e == 1) {
            play3d(S_CASSETTE, p, 0.9f, 1.0f, 8);
            int k = niveauNum - 2;                        /* niveau 2 : cassette 1 ... */
            if (k < 0) k = 0;
            if (k > NTEXTES_CASSETTES - 1) k = NTEXTES_CASSETTES - 1;
            if (netMode == NET_OFF) carton_ouvrir(texteCassettes[k], titreCassettes[k], 0);   /* seul : on la regarde */
            else message(texteCassetteCourt[k], 12.0f);   /* en ligne : le texte defile (le monde ne s'arrete pas) */
        }
        break;
    default: break;
    }
}

/* quelqu'un (une cible) voit-il ce point ? (lampe, ou ligne de vue et regard tourne vers lui) */
static int vu_par_quelquun(V3 p) {
    for (int i = 0; i < MAXP; i++) {
        const Cible *c = &cib[i];
        if (!c->actif || c->mort) continue;
        float dx = p.x - c->pos.x, dz = p.z - c->pos.z, d = sqrtf(dx * dx + dz * dz);
        if (d > 16 || !visible_xz(c->pos.x, c->pos.z, p.x, p.z)) continue;
        if (d < 1.2f) return 1;
        V3 f = view_dir(c->lampeYaw, 0);                  /* le regard (approche par la lampe, qui suit le regard) */
        if ((dx * f.x + dz * f.z) / d > 0.6f) return 1;
    }
    return 0;
}

/* l'hote (ou le solo) fait vivre les pieces */
static void pieces_tick(float dt) {
    if (netMode == NET_CLIENT) return;
    cibles_maj();
    for (int ip = 0; ip < npieces; ip++) {
        Piece *p = &pieces[ip];
        if (!p->vue) {
            for (int k = 0; k < MAXP; k++)
                if (cib[k].actif && !cib[k].mort && piece_ou(cib[k].pos.x, cib[k].pos.z) == ip) { p->vue = 1; p->t = 0; }
            if (!p->vue) continue;
            if (p->zone == ZONE_NOE && piece_ou(P.pos.x, P.pos.z) == ip) message("Un campement. Quelqu'un a dormi ici. Noe ?", 5.0f);
        }
        float t0 = p->t;
        p->t += dt;
        for (int i = 0; i < nmeubles; i++) {
            Meuble *m = &meubles[i];
            if (m->piece != ip) continue;
            m->t += dt;
            V3 q = v3(m->x, 1.0f, m->z);
            switch (m->type) {
            case MB_TELE:                                 /* elle s'eteint... puis un visage, deux images */
                if (t0 < 2.5f && p->t >= 2.5f) meuble_etat(i, 0);
                if (t0 < 4.0f && p->t >= 4.0f) meuble_etat(i, 2);
                if (m->etat == 2 && m->t > 0.14f) meuble_etat(i, 1);
                break;
            case MB_BAIGNOIRE:                            /* quelqu'un derriere le rideau... qui n'est plus la quand on eclaire */
                if (t0 < 0.6f && p->t >= 0.6f) meuble_etat(i, 1);
                if (m->etat == 1 && (dans_faisceau(q) || m->t > 9)) meuble_etat(i, 0);
                break;
            case MB_LIT:                                  /* une forme sous les draps, qui disparait dans la lumiere */
                if (m->etat == 1 && dans_faisceau(v3(m->x, 0.5f, m->z))) meuble_etat(i, 0);
                break;
            case MB_BOITE_MUSIQUE: if (t0 < 1.0f && p->t >= 1.0f) meuble_etat(i, 1); break;
            case MB_JOUETS:
                if (t0 < 5.0f && p->t >= 5.0f) play3d(S_JOUET, v3(m->x, 0.2f, m->z), 1.0f, 1.0f, 12);   /* un jouet tombe, la-bas */
                break;
            case MB_FRIGO:                                /* la porte s'ouvre, lentement */
                if (p->t > 3.0f && m->etat < 100) meuble_etat(i, m->etat + 1 + (m->etat == 0));
                break;
            case MB_MANNEQUIN: {                          /* ils bougent quand personne ne regarde */
                if (fmodf(p->t, 0.7f) >= fmodf(t0, 0.7f)) break;
                if (vu_par_quelquun(v3(m->x, 1.2f, m->z)) || frand() > 0.35f) break;
                int k = ((m->etat / 8) + 1 + (frand() < 0.5f)) % 3;
                V3 cible = P.pos;                         /* tournes vers le joueur le plus proche */
                float bd = 1e9f;
                for (int c = 0; c < MAXP; c++) if (cib[c].actif && !cib[c].mort && dist_xz(cib[c].pos, v3(m->px[k], 0, m->pz[k])) < bd) { bd = dist_xz(cib[c].pos, v3(m->px[k], 0, m->pz[k])); cible = cib[c].pos; }
                int dir = ((int)floorf(atan2f(cible.x - m->px[k], -(cible.z - m->pz[k])) / (PI / 4) + 0.5f) % 8 + 8) % 8;
                if (vu_par_quelquun(v3(m->px[k], 1.2f, m->pz[k]))) break;   /* pas sous les yeux de quelqu'un non plus */
                meuble_etat(i, k * 8 + dir);
                break;
            }
            default: break;
            }
        }
    }
}

/* un meuble qu'on peut prendre ou ouvrir devant (pos, yaw) : cassette, armoire ; -1 sinon */
static int meuble_vise_de(V3 pos, float yaw, int type) {
    V3 f = view_dir(yaw, 0);
    int best = -1; float bd = type == MB_ARMOIRE ? 1.3f : RAMASSER_DIST;
    for (int i = 0; i < nmeubles; i++) {
        const Meuble *m = &meubles[i];
        if (m->type != type || (type == MB_CASSETTE && m->etat)) continue;
        float dx = m->x - pos.x, dz = m->z - pos.z, l = sqrtf(dx * dx + dz * dz);
        if (l < bd && (l < 0.7f || (dx * f.x + dz * f.z) / l > 0.4f)) { bd = l; best = i; }
    }
    return best;
}

/* E devant la cassette : elle est prise (hote ou solo ; les clients le voient dans l'instantane) */
static int pieces_ramasser(int k, V3 pos, float yaw) {
    (void)k;
    int i = meuble_vise_de(pos, yaw, MB_CASSETTE);
    if (i < 0) return 0;
    meuble_etat(i, 1);
    return 1;
}

/* chaque image : les deux sources d'ambiance les plus proches (tele qui neige, frigo) jouent en boucle, en 3D */
static void pieces_sons(float dt) {
    if (!audio_ok) return;
    if (appState != APP_GAME || cartonActif) {
        for (int k = 0; k < 2; k++) if (voixBoucle[k]) { loop_stop(voixBoucle[k]); voixBoucle[k] = 0; srcBoucle[k] = -1; }
        return;
    }
    if ((boucleChoixT -= dt) <= 0) {
        boucleChoixT = 0.8f;
        int choix[2] = { -1, -1 }; float d[2] = { 1e9f, 1e9f };
        for (int i = 0; i < nmeubles; i++) {
            const Meuble *m = &meubles[i];
            if (!((m->type == MB_TELE && m->etat) || m->type == MB_FRIGO)) continue;
            float di = dist_xz(v3(m->x, 0, m->z), P.pos);
            if (di > 14) continue;
            if (di < d[0]) { d[1] = d[0]; choix[1] = choix[0]; d[0] = di; choix[0] = i; }
            else if (di < d[1]) { d[1] = di; choix[1] = i; }
        }
        for (int k = 0; k < 2; k++) {
            if (choix[k] == srcBoucle[k]) continue;
            if (voixBoucle[k]) { loop_stop(voixBoucle[k]); voixBoucle[k] = 0; }
            srcBoucle[k] = choix[k];
            if (choix[k] >= 0) voixBoucle[k] = play_loop(meubles[choix[k]].type == MB_TELE ? S_TELE : S_FRIGO, 0, 1.0f);
        }
    }
    for (int k = 0; k < 2; k++) {
        if (srcBoucle[k] < 0 || !voixBoucle[k]) continue;
        const Meuble *m = &meubles[srcBoucle[k]];
        float v = m->type == MB_TELE ? (m->etat ? 0.45f : 0) : 0.3f;
        loop3d(voixBoucle[k], v3(m->x, 0.8f, m->z), v, 8);
    }
    for (int ip = 0; ip < npieces; ip++)                  /* la salle de bain goutte quand on y est */
        if (pieces[ip].zone == ZONE_SDB && piece_ou(P.pos.x, P.pos.z) == ip && (goutteT -= dt) <= 0) {
            goutteT = 1.5f + frand() * 2.5f;
            play3d(S_GOUTTE, piece_pt(&pieces[ip], 0.3f, 0.4f), 0.6f, 0.9f + 0.3f * frand(), 6);
        }
}
