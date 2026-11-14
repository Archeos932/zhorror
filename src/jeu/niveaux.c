/* BOURDONNEMENT - niveaux et objectif : composition de chaque niveau (monstres, pieges, noirceur), fusibles,
   faux fusibles, machoires, ascenseur, passage au niveau suivant, messages a l'ecran
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).

   Niveau 1 : la carte faite a la main (niveau_essai.c). Ensuite : generes par graine, plus grands, plus sombres,
   plus de pieges, et des monstres qui se melangent. Objectif : FUSIBLES_N fusibles -> courant -> ascenseur. */

static int niveauNum = 1;
static int piegeSeq[MAXP];                                /* hote : nombre de fois ou chaque joueur a ete pris (instantanes) */
static float niveauFog = FOG_DENSITE;
static float niveauT;                                     /* temps passe dans le niveau (hors cartons) */
static int radioDite;                                     /* bits : messages radio deja dits */
static void monde_reconstruire(void);                     /* rendu/monde.c : lightmaps et listes du nouveau niveau */
static void peur_reset(void);                             /* jeu/screamers.c */

/* message au centre bas de l'ecran (sous-titre) */
static char msgTexte[160];
static float msgT;
static void message(const char *s, float duree) { snprintf(msgTexte, sizeof msgTexte, "%s", s); msgT = duree; }

static float sortieT = -1;                                /* >= 0 : dans la cabine, portes qui se ferment */
static int fusPris;

/* qui hante le niveau n : N1 Rodeur, N2 Locataire, N3 Rampante, N4 Rodeur + Rampante, N5 Rodeur + Locataire,
   ensuite les trois */
static int niveau_monstres(int n, int types[3]) {
    switch (n) {
    case 1: types[0] = MON_RODEUR; return 1;
    case 2: types[0] = MON_LOCATAIRE; return 1;
    case 3: types[0] = MON_RAMPANTE; return 1;
    case 4: types[0] = MON_RODEUR; types[1] = MON_RAMPANTE; return 2;
    case 5: types[0] = MON_RODEUR; types[1] = MON_LOCATAIRE; return 2;
    default: types[0] = MON_RODEUR; types[1] = MON_LOCATAIRE; types[2] = MON_RAMPANTE; return 3;
    }
}

/* le joueur local, neuf, a sa place au depart du niveau */
static void joueur_au_depart(void) {
    player_reset();
    if (netMode != NET_OFF) {                             /* en ligne : chacun a sa place a cote du depart */
        P.pos.x += (monSlot % 2 ? 0.6f : -0.6f) * (monSlot > 0); P.pos.z += monSlot >= 2 ? 0.6f : 0;
        carte_collision(&P.pos.x, &P.pos.z, RAYON_JOUEUR);
        P.prevPos = P.pos;
    }
}

static void niveau_charger_ex(int n, int avecCarton) {
    niveauNum = n;
    if (n <= 1) {
        niveauMortsPM = 300; niveauFousPM = 80;
        charger_niveau_essai();
        niveauFog = FOG_DENSITE;
    } else {
        int k = n - 2;
        niveauMortsPM = 340 + 40 * k < 540 ? 340 + 40 * k : 540;
        niveauFousPM = 80;
        genEau = n == 3 ? 16 : 3 + k;                     /* le sous-sol humide est inonde */
        genVerre = 3 + k; genMachoires = k < 2 ? 1 : k; genAlarmes = k < 2 ? 0 : k / 2;   /* les premiers restent simples */
        {                                                 /* pieces a theme : 2 ou 3 au hasard (selon n), + le campement de Noe */
            static const int pool[5] = { ZONE_CHAMBRE, ZONE_SDB, ZONE_ENFANT, ZONE_CUISINE, ZONE_RESERVE };
            unsigned h = hash32((unsigned)n * 2654435761u);
            int nz = 2 + (int)(h % 2), debut = (int)((h >> 8) % 5);
            genNZones = 0;
            for (int i = 0; i < nz; i++) genZones[genNZones++] = pool[(debut + i * 2) % 5];
            if (n >= 2 && n <= 6) genZones[genNZones++] = ZONE_NOE;   /* une cassette par niveau de l'histoire */
            genZones[genNZones++] = ZONE_CAMERAS;         /* toujours une salle des cameras */
        }
        int taille = CARTE_GEN_MIN + 3 * k < 46 ? CARTE_GEN_MIN + 3 * k : 46;   /* petits : on s'y perd moins, on s'y croise plus */
        generer_niveau(1000u + (unsigned)n * 7919u, taille, taille);
        niveauFog = FOG_DENSITE * (1.0f + 0.1f * (k + 1));
    }
    joueur_au_depart();
    monstres_reset();
    monstresEveilles = n > 1;                             /* niveau 1 : il dort jusqu'au premier fusible */
    int types[3], nt = niveau_monstres(n, types);
    for (int i = 0; i < nt; i++) mon_ajouter(types[i], 1.0f + RODEUR_NIVEAU_K * (n - 1));
    peur_reset();
    fusPris = 0; sortieT = -1; niveauT = 0; radioDite = 0;
    memset(piegeSeq, 0, sizeof piegeSeq);
    msgT = 0;
    if (avecCarton) carton_niveau(n);
    monde_reconstruire();
}
static void niveau_charger(int n) { niveau_charger_ex(n, 1); }

static void dire_radio(int k) {
    if (radioDite & (1 << k)) return;
    radioDite |= 1 << k;
    message(radio(niveauNum, k), 7.5f);
    play(S_RADIO, 0.55f, 1.0f);
}

/* un objet a ramasser devant (pos, yaw) ? (fusible : 0..nfus-1 ; faux fusible : 100 + indice du piege) */
static int objet_vise_de(V3 pos, float yaw) {
    V3 f = view_dir(yaw, 0);
    int best = -1; float bd = RAMASSER_DIST;
    for (int i = 0; i < nfus + npieges; i++) {
        V3 p;
        if (i < nfus) { if (fus[i].pris) continue; p = fus[i].pos; }
        else { Piege *pg = &pieges[i - nfus]; if (pg->type != PIEGE_ALARME || !pg->arme) continue; p = pg->pos; }
        V3 d = vsub(p, pos); d.y = 0;
        float l = hlen(d);
        if (l < bd && (l < 0.8f || (d.x * f.x + d.z * f.z) / l > 0.3f)) { bd = l; best = i < nfus ? i : 100 + i - nfus; }
    }
    return best;
}
static int objet_vise(void) { return objet_vise_de(P.pos, P.yaw); }


static int pieces_ramasser(int k, V3 pos, float yaw);     /* jeu/pieces.c : la cassette de Noe */

/* le joueur k (place) ramasse ce qu'il vise ; local : effets a l'ecran */
static void ramasser(int k, V3 pos, float yaw) {
    int local = k == slot_local(), o = objet_vise_de(pos, yaw);
    if (o < 0) { pieces_ramasser(k, pos, yaw); return; }
    if (o >= 100) {                                       /* un faux : il hurle */
        Piege *pg = &pieges[o - 100];
        pg->arme = 0; pg->t = 0;
        if (local) { sursauter(BPM_SURSAUT); message("C'ETAIT UN LEURRE.  Eloigne-toi de l'alarme.", 4.0f); }
    } else if (o >= 0) {
        fus[o].pris = 1; fusPris++;
        if (local) { play(S_FUSIBLE, 0.7f, 1.0f); faire_bruit(BRUIT_RAMASSER); }
        else play3d(S_FUSIBLE, fus[o].pos, 0.8f, 1.0f, 12);
        if (fusPris == 1) dire_radio(1);
        if (fusPris < nfus) {
            char m[64];
            if (local) snprintf(m, sizeof m, "FUSIBLE %d / %d", fusPris, nfus);
            else snprintf(m, sizeof m, "%s a trouve un fusible (%d / %d)", JP[k].nom, fusPris, nfus);
            if (fusPris > 1 || !local) message(m, 3.0f);
        } else {
            asc.alimente = 1;
            dire_radio(2);
            play3d(S_DING, v3(asc.gx + 0.5f, 1.5f, asc.gz + 0.5f), 1.2f, 1.0f, 30);
        }
    }
}

/* effets locaux d'une machoire (le joueur local vient d'etre pris) */
static void pris_au_piege(void) {
    P.piegeT = MACHOIRE_T;
    play(S_MACHOIRE, 1.2f, 1.0f); play(S_CRI, 0.9f, 0.9f);
    faire_bruit(BRUIT_MACHOIRE);
    sursauter(BPM_SURSAUT * 1.3f);
    message("PIEGE !  martele [E] pour t'en sortir", 3.0f);
}

static void objectif_tick(float dt) {
    if (msgT > 0) msgT -= dt;
    niveauT += dt;
    if (niveauT > 3.5f) dire_radio(0);
    if (!monstresEveilles && (fusPris > 0 || niveauT > 90)) {   /* il se reveille, quelque part, et il hurle */
        monstresEveilles = 1;
        if (nmon) play3d(S_RUGISSEMENT, mon[0].pos, 1.6f, 0.75f, 60);
        message("Quelque chose s'est reveille.", 4.0f);
        sursauter(BPM_SURSAUT);
    }
    if (asc.actif) asc.dedans = (int)floorf(P.pos.x) == asc.gx && (int)floorf(P.pos.z) == asc.gz;
    if (netMode == NET_CLIENT) return;                    /* le reste : decide par l'hote (instantanes) */
    cibles_maj();

    for (int i = 0; i < npieges; i++) {                   /* pieges */
        Piege *pg = &pieges[i];
        if (pg->t >= 0) pg->t += dt;
        if (pg->type == PIEGE_MACHOIRE && pg->arme)
            for (int k = 0; k < MAXP; k++) {
                if (!cib[k].actif || cib[k].mort || dist_xz(pg->pos, cib[k].pos) >= 0.42f) continue;
                pg->arme = 0; pg->t = 0;
                piegeSeq[k]++;
                if (k == slot_local()) pris_au_piege();
                else play3d(S_MACHOIRE, pg->pos, 1.2f, 1.0f, 25);
                break;
            }
        if (pg->type == PIEGE_ALARME && pg->t >= 0 && pg->t < ALARME_T) {   /* le faux fusible sonne */
            if ((int)(pg->t * 3) != (int)((pg->t - dt) * 3)) play3d(S_ALARME, v3(pg->pos.x, 0.3f, pg->pos.z), 1.0f, 1.0f, 30);
            bruit_monde(pg->pos, BRUIT_ALARME);
        }
    }

    if (actP[A_UTILISER] && P.piegeT <= 0 && !P.dead) ramasser(slot_local(), P.pos, P.yaw);   /* ramasser */
    if (netMode == NET_HOTE)
        for (int k = 0; k < MAXP; k++) {                  /* ... et les autres joueurs (front montant de "utilise") */
            if (k == monSlot || !JP[k].actif) continue;
            int u = (JP[k].st.flags & JF_UTILISE) != 0;
            if (u && !JP[k].utiliseVu && !(JP[k].st.flags & (JF_MORT | JF_PIEGE))) ramasser(k, v3(JP[k].st.x, 0, JP[k].st.z), JP[k].st.yaw);
            JP[k].utiliseVu = u;
        }

    if (!asc.actif) return;
    V3 ac = v3(asc.gx + 0.5f, 0, asc.gz + 0.5f);
    int vivants = 0, dedans = 0, pres = 0;                /* on part quand TOUS les vivants sont au fond de la cabine */
    for (int k = 0; k < MAXP; k++) {
        if (!cib[k].actif || cib[k].mort) continue;
        float d = dist_xz(cib[k].pos, ac);
        vivants++;
        if ((int)floorf(cib[k].pos.x) == asc.gx && (int)floorf(cib[k].pos.z) == asc.gz && d < 0.45f) dedans++;
        if (d < 3.2f) pres = 1;
    }
    if (sortieT < 0) {
        float voulu = asc.alimente && pres ? 1.0f : 0.0f;   /* les portes s'ouvrent quand quelqu'un approche */
        if (voulu > asc.ouverture && asc.ouverture == 0) play3d(S_PORTE, ac, 0.8f, 1.0f, 15);
        if (voulu > asc.ouverture) asc.ouverture = fminf(1, asc.ouverture + dt / ASC_PORTE_T);
        else if (voulu < asc.ouverture) asc.ouverture = fmaxf(0, asc.ouverture - dt / ASC_PORTE_T);
        if (asc.alimente && vivants > 0 && dedans == vivants) {   /* tout le monde est la : on part */
            sortieT = 0;
            play(S_PORTE, 0.8f, 0.9f);
        } else if (asc.alimente && dedans > 0 && vivants > 1 && msgT <= 0) message("L'ascenseur attend les autres...", 2.0f);
    } else {
        sortieT += dt;
        asc.ouverture = fmaxf(0, 1 - sortieT / ASC_PORTE_T);
        if (sortieT > ASC_PORTE_T && sortieT - dt <= ASC_PORTE_T) play(S_DING, 0.8f, 0.94f);
        if (sortieT > ASC_PORTE_T + 2.2f) niveau_charger(niveauNum + 1);
    }
}
