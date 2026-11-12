/* BOURDONNEMENT - le jeu en ligne : salons (serveur Cloudflare, worker/) et synchronisation de la partie
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).

   Salons (lignes texte, voir worker/src/salons.js) : nom, liste, creer, rejoindre, quitter, lancer.
   Partie : tout passe par le serveur (relais).
     client -> hote : "d <b64>"        = son etat (PaquetJ), 15 fois par seconde ;
     hote -> tous   : "d * <b64>"      = l'instantane (Snap), 15 fois par seconde.
   Le WebSocket est fiable et ordonne : pas de renvoi. Les evenements (fusible pris, piege qui claque, mort,
   changement de niveau) se deduisent des differences entre deux instantanes. L'hote decide de tout ce qui est
   partage : monstres, pieges, fusibles, ascenseur, morts, niveau. Chacun simule son propre deplacement. */

static void reset_game(void);                             /* jeu/boucle.c */
static int autoMulti;                                     /* plateforme/demarrage.c : --multi (tests) */

#define RESEAU_VER 2                                      /* a incrementer des qu'un paquet change */
#define RESEAU_HZ 15.0f
enum { SAL_AUCUN, SAL_CONNEXION, SAL_LISTE, SAL_DANS, SAL_JEU };
static int salEtat, salHote;
static char salCode[8], codeSaisi[8];
typedef struct { char code[8], hote[16]; int n, max, enJeu; } SalonVu;
#define MAXSALONS 16
static SalonVu salons[MAXSALONS];
static int nsalons, nsalonsRecus;
static SalonVu salonsRecus[MAXSALONS];
static float salListeT, salEnvoiT;
static int essaiNiveau, essaiVuClient;                    /* hote : compteur de rechargements ; client : celui charge */
static int mortHote[MAXP];                                /* hote : type + 1 du monstre qui a tue chaque joueur */
static float tousMortsT;
static int netRecus, netEnvoyes;                          /* compteurs (journal, tests) */

#define MEUBLES_RESEAU 48
#pragma pack(push, 1)
typedef struct { unsigned char type, ver, pad[2]; NetJ j; } PaquetJ;
typedef struct { float x, z, yaw, anim; unsigned char type, etat, actif, pad; } NetMon;
typedef struct {
    unsigned char type, ver, niveau, essai;
    float gameT, sortieT;
    unsigned char ouverture, alimente, nmon, actifs;
    unsigned int fusMask;
    unsigned long long piegeArme;
    unsigned char piegeSeq[MAXP];
    NetJ j[MAXP];
    NetMon m[MAXMON];
    unsigned char meubleEtat[MEUBLES_RESEAU];             /* les meubles vivants (tele, frigo, mannequins, cassette...) */
} Snap;
#pragma pack(pop)
typedef char snap_tient[((sizeof(Snap) + 2) / 3 * 4 + 16 < WS_LIGNE) ? 1 : -1];   /* l'instantane tient dans une ligne */

static NetMon monNet[MAXMON];                             /* client : derniere position recue de chaque monstre */
static int monNetEtatVu[MAXMON];

/* ---- l'hote fabrique l'instantane ---------------------------------------------------------------- */
static void snap_faire(Snap *s) {
    memset(s, 0, sizeof *s);
    s->type = 'S'; s->ver = RESEAU_VER; s->niveau = (unsigned char)niveauNum; s->essai = (unsigned char)essaiNiveau;
    s->gameT = gameT; s->sortieT = sortieT;
    s->ouverture = (unsigned char)(asc.ouverture * 255); s->alimente = (unsigned char)asc.alimente;
    for (int i = 0; i < nfus && i < 32; i++) if (fus[i].pris) s->fusMask |= 1u << i;
    for (int i = 0; i < npieges && i < 64; i++) if (pieges[i].arme) s->piegeArme |= 1ull << i;
    for (int k = 0; k < MAXP; k++) {
        s->piegeSeq[k] = (unsigned char)piegeSeq[k];
        if (k == monSlot) { netj_de_P(&s->j[k]); s->actifs |= (unsigned char)(1 << k); }
        else if (JP[k].actif) { s->j[k] = JP[k].st; s->actifs |= (unsigned char)(1 << k); }
        if (mortHote[k]) { s->j[k].flags |= JF_MORT; s->j[k].mortType = (unsigned char)(mortHote[k] - 1); }
    }
    for (int i = 0, k = 0; i < nmeubles && k < MEUBLES_RESEAU; i++)
        if (meuble_vivant(meubles[i].type)) s->meubleEtat[k++] = (unsigned char)meubles[i].etat;
    s->nmon = (unsigned char)nmon;
    for (int i = 0; i < nmon; i++) {
        NetMon *m = &s->m[i];
        m->x = mon[i].pos.x; m->z = mon[i].pos.z; m->yaw = mon[i].yaw; m->anim = mon[i].anim;
        m->type = (unsigned char)mon[i].type; m->etat = (unsigned char)mon[i].etat; m->actif = (unsigned char)mon[i].actif;
    }
}

/* l'hote : un monstre a tue un autre joueur */
static void reseau_mort_distante(int slot, int type) {
    if (netMode != NET_HOTE || slot < 0 || slot >= MAXP || mortHote[slot]) return;
    mortHote[slot] = type + 1;
    char m[64]; snprintf(m, sizeof m, "%s a ete pris.", JP[slot].nom);
    message(m, 4.0f);
    netlog("mort de %s (monstre %d)", JP[slot].nom, type);
}

/* ---- le client applique l'instantane ------------------------------------------------------------ */
static void snap_appliquer(const Snap *s) {
    if (s->ver != RESEAU_VER) return;
    if (s->niveau != niveauNum || s->essai != essaiVuClient) {   /* nouveau niveau, ou on recommence */
        int nouveau = s->niveau != niveauNum;
        niveau_charger_ex(s->niveau, nouveau);
        essaiVuClient = s->essai;
        netlog("niveau %d (essai %d)", s->niveau, s->essai);
        for (int i = 0; i < MAXMON; i++) monNetEtatVu[i] = -1;
    }
    gameT = s->gameT;
    for (int i = 0, k = 0; i < nmeubles && k < MEUBLES_RESEAU; i++)   /* les meubles : memes indices chez tous */
        if (meuble_vivant(meubles[i].type)) meuble_etat(i, s->meubleEtat[k++]);
    for (int i = 0; i < nfus && i < 32; i++)              /* fusibles pris par quelqu'un */
        if ((s->fusMask >> i & 1) && !fus[i].pris) {
            fus[i].pris = 1; fusPris++;
            play3d(S_FUSIBLE, fus[i].pos, 0.8f, 1.0f, 12);
            if (fusPris == 1) dire_radio(1);
            if (fusPris >= nfus) { dire_radio(2); play3d(S_DING, v3(asc.gx + 0.5f, 1.5f, asc.gz + 0.5f), 1.2f, 1.0f, 30); }
            else { char m[48]; snprintf(m, sizeof m, "FUSIBLE %d / %d", fusPris, nfus); message(m, 3.0f); }
        }
    for (int i = 0; i < npieges && i < 64; i++)           /* pieges qui se declenchent */
        if (!(s->piegeArme >> i & 1) && pieges[i].arme) {
            pieges[i].arme = 0; pieges[i].t = 0;
            if (pieges[i].type == PIEGE_MACHOIRE) play3d(S_MACHOIRE, pieges[i].pos, 1.2f, 1.0f, 25);
        }
    if (s->piegeSeq[monSlot] != (unsigned char)piegeSeq[monSlot]) { piegeSeq[monSlot] = s->piegeSeq[monSlot]; pris_au_piege(); }
    asc.alimente = s->alimente; asc.ouverture = s->ouverture / 255.0f;
    if (s->sortieT >= 0 && sortieT < 0) play(S_PORTE, 0.8f, 0.9f);
    if (s->sortieT > ASC_PORTE_T && sortieT <= ASC_PORTE_T) play(S_DING, 0.8f, 0.94f);
    sortieT = s->sortieT;
    for (int k = 0; k < MAXP; k++) {
        int actif = (s->actifs >> k & 1) != 0;
        if (k == monSlot) {
            if ((s->j[k].flags & JF_MORT) && !P.dead && screamT < 0) { tueur = -1; mort_commencer(s->j[k].mortType); }
            continue;
        }
        if (actif && JP[k].actif && !(JP[k].st.flags & JF_MORT) && (s->j[k].flags & JF_MORT)) {
            char m[64]; snprintf(m, sizeof m, "%s a ete pris.", JP[k].nom); message(m, 4.0f);
        }
        if (actif && !JP[k].actif) JP[k].disp = v3(s->j[k].x, 0, s->j[k].z);
        JP[k].actif = actif;
        if (actif) JP[k].st = s->j[k];
    }
    nmon = s->nmon;
    for (int i = 0; i < nmon; i++) {
        monNet[i] = s->m[i];
        Monstre *m = &mon[i];
        if (!m->actif && s->m[i].actif) { m->pos = m->prevPos = v3(s->m[i].x, 0, s->m[i].z); }
        m->actif = s->m[i].actif; m->type = s->m[i].type; m->etat = s->m[i].etat;
    }
}

/* client : les monstres glissent vers leur derniere position recue ; leurs sons sont rejoues ici */
static void monstres_client_tick(float dt) {
    for (int i = 0; i < nmon; i++) {
        Monstre *m = &mon[i];
        if (!m->actif) continue;
        m->prevPos = m->pos;
        V3 c = v3(monNet[i].x, 0, monNet[i].z);
        float d = dist_xz(m->pos, c);
        if (d > 3) m->pos = c; else m->pos = vlerp(m->pos, c, fminf(1, dt * 12));
        float dy = monNet[i].yaw - m->yaw;
        while (dy > PI) dy -= 2 * PI;
        while (dy < -PI) dy += 2 * PI;
        m->yaw += dy * fminf(1, dt * 12);
        m->anim = monNet[i].anim;
        if (monNetEtatVu[i] != m->etat) {                 /* il se lance a la poursuite : le cri, chez tout le monde */
            if (m->etat == E_CHASSE && monNetEtatVu[i] >= 0) {
                if (m->type == MON_RODEUR) play3d(S_RUGISSEMENT, m->pos, 1.3f, 0.95f, 25);
                if (m->type == MON_RAMPANTE) play3d(S_CRI_RAMPANTE, m->pos, 1.3f, 1.0f, 30);
            }
            monNetEtatVu[i] = m->etat;
        }
        float moved = hlen(vsub(m->pos, m->prevPos));
        m->pasDist += moved;
        if (m->type == MON_RODEUR && m->pasDist > 0.95f) { m->pasDist = 0; play3d(S_RODEUR_PAS, m->pos, 1.0f, 0.85f + 0.2f * frand(), 15); }
        if (m->type == MON_LOCATAIRE && m->pasDist > 0.6f) { m->pasDist = 0; play3d(S_CRAQUE, m->pos, 0.8f, 0.7f + 0.5f * frand(), 14); }
        if (m->type == MON_RAMPANTE && m->pasDist > 0.7f) { m->pasDist = 0; play3d(S_GRATTE, m->pos, 0.9f, 0.9f + 0.3f * frand(), 13); }
    }
    for (int i = 0; i < npieges; i++) {                   /* un faux fusible qui sonne */
        Piege *pg = &pieges[i];
        if (pg->type != PIEGE_ALARME || pg->t < 0 || pg->t > ALARME_T) continue;
        if ((int)(pg->t * 3) != (int)((pg->t + dt) * 3)) play3d(S_ALARME, v3(pg->pos.x, 0.3f, pg->pos.z), 1.0f, 1.0f, 30);
        pg->t += dt;
    }
}

/* ---- les autres joueurs : affichage lisse, pas, cris -------------------------------------------- */
static void joueurs_tick(float dt) {
    for (int k = 0; k < MAXP; k++) {
        Joueur *j = &JP[k];
        if (!j->actif || k == monSlot) continue;
        V3 c = v3(j->st.x, 0, j->st.z);
        if (dist_xz(j->disp, c) > 3) j->disp = c; else j->disp = vlerp(j->disp, c, fminf(1, dt * 14));
        if (j->st.pas != j->pasVu) {
            int eau = sol_en(c.x, c.z) == SOL_EAU, verre = sol_en(c.x, c.z) == SOL_VERRE;
            play3d(eau ? S_FLAQUE : verre ? S_VERRE : S_PAS, c, (j->st.flags & JF_ACCROUPI) ? 0.25f : 0.6f, 0.9f + 0.2f * frand(), 10);
            j->pasVu = j->st.pas;
        }
        if (j->criT > 0) j->criT -= dt;
        if ((j->st.flags & JF_CRI) && j->criT <= 0) { play3d(S_CRI, c, 1.0f, 0.9f + 0.2f * frand(), 25); j->criT = 1.0f; }
    }
}

/* ---- salons ------------------------------------------------------------------------------------- */
static void nom_propre(char *out, int n) {                /* le serveur n'accepte que [A-Za-z0-9_.-] */
    int k = 0;
    for (const char *c = monNom; *c && k < n - 1; c++)
        if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_' || *c == '-' || *c == '.') out[k++] = *c;
    if (!k) out[k++] = 'X';
    out[k] = 0;
}

static void en_ligne_ouvrir(void) {                       /* ecran JOUER EN LIGNE */
    ws_demarrer();
    salEtat = SAL_CONNEXION; nsalons = 0; salListeT = 0;
}

static void en_ligne_fermer(void) {
    if (salEtat == SAL_DANS || salEtat == SAL_JEU) ws_envoyer("quitter");
    ws_arreter();
    salEtat = SAL_AUCUN; netMode = NET_OFF; monSlot = 0; salHote = 0; salCode[0] = 0;
    memset(JP, 0, sizeof JP);
}

static void salon_creer(void) { ws_envoyer("creer %d %d", MAXP, RESEAU_VER); }
static void salon_rejoindre(const char *code) { ws_envoyer("rejoindre %s %d", code, RESEAU_VER); }
static void salon_lancer(void) { if (salHote) ws_envoyer("lancer"); }

static void partie_lancer(void) {                         /* "lance" : tout le monde part ensemble */
    if (salEtat == SAL_JEU) return;                       /* deja partis */
    netMode = salHote ? NET_HOTE : NET_CLIENT;
    salEtat = SAL_JEU;
    essaiNiveau = 0; essaiVuClient = 0;
    memset(mortHote, 0, sizeof mortHote);
    for (int k = 0; k < MAXP; k++) JP[k].utiliseVu = 0;
    reset_game();
    appState = APP_GAME;
    if (!autoMulti) set_capture(1);
    netlog("partie lancee (%s, place %d)", salHote ? "hote" : "client", monSlot);
}

static void retour_menu(const char *pourquoi) {
    en_ligne_fermer();
    set_capture(0);
    appState = APP_MENU;
    if (pourquoi) set_status(pourquoi);
    netlog("retour au menu : %s", pourquoi ? pourquoi : "");
}

/* une ligne du serveur */
static void serveur_ligne(char *l) {
    if (l[0] == 'd' && l[1] == ' ') {                     /* paquet du jeu */
        unsigned char b[sizeof(Snap) + 16];
        char *p = l + 2;
        int de = -1;
        if (netMode == NET_HOTE) { de = atoi(p); p = strchr(p, ' '); if (!p) return; p++; }
        int n = b64_dec(p, b, sizeof b);
        netRecus++;
        if (netMode == NET_HOTE && n == (int)sizeof(PaquetJ) && de > 0 && de < MAXP) {
            PaquetJ pk; memcpy(&pk, b, sizeof pk);
            if (pk.type == 'J' && pk.ver == RESEAU_VER) { JP[de].st = pk.j; if (!JP[de].actif) JP[de].disp = v3(pk.j.x, 0, pk.j.z); JP[de].actif = 1; }
        } else if (netMode == NET_CLIENT && n == (int)sizeof(Snap)) {
            Snap s; memcpy(&s, b, sizeof s);
            if (s.type == 'S') snap_appliquer(&s);
        }
        return;
    }
    netlog("<< %s", l);
    char *w = strtok(l, " ");
    if (!w) return;
    if (!strcmp(w, "bonjour")) {
        char nom[16]; nom_propre(nom, sizeof nom);
        ws_envoyer("nom %s", nom);
        salEtat = SAL_LISTE;
        ws_envoyer("liste %d", RESEAU_VER);
        salListeT = 3;
    } else if (!strcmp(w, "salon") && nsalonsRecus < MAXSALONS) {   /* salon <code> <n> <max> <etat> <hote> */
        SalonVu *s = &salonsRecus[nsalonsRecus];
        char *c = strtok(NULL, " "), *n = strtok(NULL, " "), *mx = strtok(NULL, " "), *e = strtok(NULL, " "), *h = strtok(NULL, " ");
        if (c && n && mx && e && h) {
            snprintf(s->code, sizeof s->code, "%s", c); snprintf(s->hote, sizeof s->hote, "%s", h);
            s->n = atoi(n); s->max = atoi(mx); s->enJeu = !strcmp(e, "jeu");
            nsalonsRecus++;
        }
    } else if (!strcmp(w, "fin")) {
        memcpy(salons, salonsRecus, sizeof salons); nsalons = nsalonsRecus; nsalonsRecus = 0;
    } else if (!strcmp(w, "cree") || !strcmp(w, "rejoint")) {   /* cree <code> | rejoint <code> <place> */
        char *c = strtok(NULL, " "), *sl = strtok(NULL, " ");
        if (!c) return;
        snprintf(salCode, sizeof salCode, "%s", c);
        salHote = !strcmp(w, "cree");
        monSlot = salHote ? 0 : (sl ? atoi(sl) : 1);
        salEtat = SAL_DANS;
        memset(JP, 0, sizeof JP);
    } else if (!strcmp(w, "membres")) {                   /* membres <place>:<nom> ... */
        int vu[MAXP] = { 0 };
        for (char *t = strtok(NULL, " "); t; t = strtok(NULL, " ")) {
            char *c = strchr(t, ':');
            if (!c) continue;
            int k = atoi(t);
            if (k < 0 || k >= MAXP) continue;
            vu[k] = 1;
            if (!JP[k].actif && salEtat == SAL_JEU && k != monSlot) { char m[64]; snprintf(m, sizeof m, "%s a rejoint la partie.", c + 1); message(m, 3); }
            snprintf(JP[k].nom, sizeof JP[k].nom, "%s", c + 1);
            if (salEtat != SAL_JEU) JP[k].actif = 1;
        }
        for (int k = 0; k < MAXP; k++)
            if (!vu[k] && JP[k].nom[0]) {
                if (salEtat == SAL_JEU && k != monSlot) { char m[64]; snprintf(m, sizeof m, "%s a quitte la partie.", JP[k].nom); message(m, 3); }
                memset(&JP[k], 0, sizeof JP[k]);
            }
    } else if (!strcmp(w, "lance")) {
        partie_lancer();
    } else if (!strcmp(w, "refuse")) {
        char *r = strtok(NULL, "");
        char m[96]; snprintf(m, sizeof m, "impossible de rejoindre : %s", r ? r : "?");
        set_status(m);
        if (salEtat != SAL_JEU) salEtat = SAL_LISTE;
    } else if (!strcmp(w, "ferme")) {                     /* l'hote est parti */
        if (salEtat == SAL_JEU) retour_menu("l'hote a quitte la partie");
        else { salEtat = SAL_LISTE; salCode[0] = 0; set_status("l'hote a ferme le salon"); }
    }
}

/* chaque image : lignes du serveur, envois periodiques, liste des salons */
static void reseau_maj(float dt) {
    if (salEtat == SAL_AUCUN) return;
    char l[WS_LIGNE];
    for (int n = 0; n < 64 && ws_ligne(l); n++) serveur_ligne(l);
    if (salEtat == SAL_AUCUN) return;                     /* une ligne nous a renvoyes au menu */
    if (wsEtat == WS_ECHEC) {
        if (salEtat == SAL_JEU) retour_menu("connexion au serveur perdue");
        else { en_ligne_fermer(); appState = APP_MENU; set_status("serveur injoignable (connexion ? serveur= dans bourdonnement.ini ?)"); }
        return;
    }
    if (salEtat == SAL_LISTE && (salListeT -= dt) <= 0) { ws_envoyer("liste %d", RESEAU_VER); salListeT = 3; }
    if (salEtat != SAL_JEU) return;
    if ((salEnvoiT -= dt) > 0) return;
    salEnvoiT += 1.0f / RESEAU_HZ;
    if (salEnvoiT < 0) salEnvoiT = 0;
    if (netMode == NET_HOTE) {
        Snap s; char b[WS_LIGNE];
        snap_faire(&s);
        b64_enc((const unsigned char *)&s, sizeof s, b);
        if (ws_envoyer("d * %s", b)) netEnvoyes++;
    } else if (netMode == NET_CLIENT) {
        PaquetJ pk; char b[WS_LIGNE];
        memset(&pk, 0, sizeof pk);
        pk.type = 'J'; pk.ver = RESEAU_VER;
        netj_de_P(&pk.j);
        b64_enc((const unsigned char *)&pk, sizeof pk, b);
        if (ws_envoyer("d %s", b)) netEnvoyes++;
    }
}

/* mort en ligne : le joueur que l'on regarde (-1 : personne, ecran de mort) */
static int spectateur(void) {
    if (netMode == NET_OFF || !P.dead || mortT < 2.5f) return -1;
    int v[MAXP], n = 0;
    for (int k = 0; k < MAXP; k++) if (k != monSlot && JP[k].actif && !(JP[k].st.flags & JF_MORT)) v[n++] = k;
    return n ? v[(int)spectateurK % n] : -1;
}

/* hote : tous les joueurs sont morts -> on recommence le niveau (tout le monde suit par l'instantane) */
static void reseau_hote_tick(float dt) {
    static int niveauVu = -1, essaiVu = -1;
    if (netMode != NET_HOTE) return;
    if (niveauNum != niveauVu || essaiNiveau != essaiVu) { memset(mortHote, 0, sizeof mortHote); niveauVu = niveauNum; essaiVu = essaiNiveau; }
    int vivants = 0, presents = 0;
    for (int k = 0; k < MAXP; k++) {
        int moi = k == monSlot;
        if (!moi && !JP[k].actif) continue;
        presents++;
        if (!(moi ? P.dead : mortHote[k] != 0)) vivants++;
    }
    if (presents && !vivants) {
        if ((tousMortsT += dt) > 5.0f) {
            tousMortsT = 0;
            memset(mortHote, 0, sizeof mortHote);
            essaiNiveau++;
            niveau_charger_ex(niveauNum, 0);
        }
    } else tousMortsT = 0;
}
