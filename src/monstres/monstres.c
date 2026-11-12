/* BOURDONNEMENT - les monstres. Trois especes, trois regles qui se contredisent :
     le RODEUR    : aveugle, il n'entend que le bruit            -> se taire (accroupi, eviter l'eau et le verre)
     le LOCATAIRE : il n'avance que dans le noir                 -> le garder dans la lumiere (lampe ou neon)
     la RAMPANTE  : elle fonce sur la lumiere, plus vite que toi -> eteindre la lampe
   Un niveau peut en melanger plusieurs : garder la lampe sur le Locataire attire la Rampante...
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). L'hote les simulera (etape 4). */

enum { MON_RODEUR, MON_LOCATAIRE, MON_RAMPANTE, MON_TYPES };
enum { E_ERRE, E_ECOUTE, E_ENQUETE, E_CHASSE, E_FIGE };
typedef struct {
    int actif, type, etat, entendu;
    V3 pos, prevPos, cible;
    float yaw, etatT, repathT, oubliT, pasDist, anim, vitK, bloqueT, vivT, sonT;
    int souffle;                                          /* voix en boucle (respiration, raclement) */
    int proie;                                            /* la cible (place du joueur) qu'il poursuit */
    short dist[CARTE_MAX][CARTE_MAX];                     /* distances vers sa cible : il descend la pente */
} Monstre;
#define MAXMON 4
static Monstre mon[MAXMON];
static int nmon;
static int tueur = -1;                                    /* le monstre qui vient d'attraper le joueur */
static int monstresEveilles = 1;                          /* niveau 1 : il dort jusqu'au premier fusible (niveaux.c) */

static float dist_xz(V3 a, V3 b) { float dx = a.x - b.x, dz = a.z - b.z; return sqrtf(dx * dx + dz * dz); }

static void mon_viser(Monstre *m, V3 c) {
    m->cible = c;
    carte_distances_dans(m->dist, (int)floorf(c.x), (int)floorf(c.z));
    m->repathT = 0.5f;
}
static void mon_etat(Monstre *m, int e, float duree) { m->etat = e; m->etatT = duree; }

/* case libre au hasard dans l'anneau [rmin, rmax] autour de p */
static int case_autour(V3 p, float rmin, float rmax, V3 *out) {
    for (int essai = 0; essai < 60; essai++) {
        float a = frand() * 2 * PI, r = rmin + frand() * (rmax - rmin);
        int gx = (int)floorf(p.x + sinf(a) * r), gz = (int)floorf(p.z + cosf(a) * r);
        if (cell_mur(gx, gz) || (asc.actif && gx == asc.gx && gz == asc.gz)) continue;
        *out = v3(gx + 0.5f, 0, gz + 0.5f);
        return 1;
    }
    return 0;
}

/* le faisceau d'une lampe (celle de n'importe quel joueur) touche-t-il p ? */
static int faisceau_de(const Cible *c, V3 p) {
    if (!c->actif || c->mort || !c->lampe) return 0;
    V3 eye = v3(c->pos.x, c->oeil, c->pos.z), to = vsub(v3(p.x, 1.2f, p.z), eye);
    float d = vlen(to);
    if (d > LAMPE_PORTEE) return 0;
    if (d < 0.8f) return 1;
    float cc = vdot(view_dir(c->lampeYaw, c->lampePitch), to) / d;
    return cc > cosf((LAMPE_ANGLE + 4) * PI / 180.0f) && visible_xz(eye.x, eye.z, p.x, p.z);
}
static int dans_faisceau(V3 p) {
    for (int i = 0; i < MAXP; i++) if (faisceau_de(&cib[i], p)) return 1;
    return 0;
}

static void mon_ajouter(int type, float vitK) {
    if (nmon >= MAXMON) return;
    Monstre *m = &mon[nmon++];
    memset(m, 0, sizeof *m);
    m->actif = 1; m->type = type; m->vitK = vitK;
    carte_distances((int)carteDepart.x, (int)carteDepart.z);   /* loin du depart (et des autres), accessible */
    int best = -1, bx = 0, bz = 0;
    for (int z = 0; z < carteH; z++)
        for (int x = 0; x < carteW; x++) {
            int d = distCase[z][x];
            if (d < 0 || (asc.actif && x == asc.gx && z == asc.gz)) continue;
            int s = d >= (int)RODEUR_DEPART_MIN ? 1000 + (int)(frand() * 1000) : d;
            for (int k = 0; k < nmon - 1; k++) if (dist_xz(mon[k].pos, v3(x + 0.5f, 0, z + 0.5f)) < 10) s -= 600;
            if (s > best) { best = s; bx = x; bz = z; }
        }
    m->pos = m->prevPos = v3(bx + 0.5f, 0, bz + 0.5f);
    mon_etat(m, E_ERRE, 0);
    mon_viser(m, m->pos);
}

static void monstres_reset(void) {
    for (int i = 0; i < nmon; i++) if (mon[i].souffle) loop_stop(mon[i].souffle);
    nmon = 0; tueur = -1;
    bruitMondeForce = 0;
}

/* avance vers sa cible a la vitesse v ; renvoie 1 s'il est arrive */
static int mon_avancer(Monstre *m, float v, float dt) {
    int gx = (int)floorf(m->pos.x), gz = (int)floorf(m->pos.z);
    V3 vers = m->cible;
    int dc = (gx >= 0 && gz >= 0 && gx < CARTE_MAX && gz < CARTE_MAX) ? m->dist[gz][gx] : -1;
    if (dc > 0) {                                         /* descendre la pente des distances, case par case */
        int best = dc, bx = gx, bz = gz;
        for (int k = 0; k < 4; k++) {
            int nx = gx + cotes[k][0], nz = gz + cotes[k][1];
            if (nx < 0 || nz < 0 || nx >= CARTE_MAX || nz >= CARTE_MAX) continue;
            int dn = m->dist[nz][nx];
            if (dn >= 0 && dn < best) { best = dn; bx = nx; bz = nz; }
        }
        vers = v3(bx + 0.5f, 0, bz + 0.5f);
        if (dist_xz(m->pos, m->cible) < 6 && visible_xz(m->pos.x, m->pos.z, m->cible.x, m->cible.z)) vers = m->cible;
    }
    V3 dir = vsub(vers, m->pos); dir.y = 0;
    float l = hlen(dir);
    if (l < 0.05f && dc <= 0) return 1;
    if (l > 0.001f) {
        float step = fminf(v * dt, l);
        m->pos = vadd(m->pos, vmul(dir, step / l));
        float ty = atan2f(dir.x, -dir.z), dy = ty - m->yaw;
        while (dy > PI) dy -= 2 * PI;
        while (dy < -PI) dy += 2 * PI;
        m->yaw += dy * fminf(1, 7 * dt);
    }
    carte_collision(&m->pos.x, &m->pos.z, 0.3f);
    return 0;
}

static void mon_errer(Monstre *m) {                       /* il erre autour d'un joueur pris au hasard */
    V3 c;
    int k = cible_hasard();
    if (k >= 0 && case_autour(cib[k].pos, RODEUR_ERRE_RMIN, RODEUR_ERRE_RMAX, &c)) mon_viser(m, c);
    mon_etat(m, E_ERRE, 0);
}

/* ---- le Rodeur -------------------------------------------------------------------------------- */
static int mon_entend_c(const Monstre *m, const Cible *c, float d) {
    if (!c->actif || c->mort || c->bruit < 0.01f) return 0;
    float portee = RODEUR_OUIE * c->bruit;
    if (!visible_xz(m->pos.x, m->pos.z, c->pos.x, c->pos.z)) portee *= 0.5f;
    return d < portee;
}
static int mon_entend(const Monstre *m, float d) { cibles_maj(); return mon_entend_c(m, &cib[slot_local()], d); }   /* le joueur local */

static float rodeur_tick(Monstre *m, float dt) {
    int best = -1; float bk = 0;                          /* celui qu'il entend le mieux (bruit / distance) */
    for (int i = 0; i < MAXP; i++) {
        float d = dist_xz(m->pos, cib[i].pos);
        if (!mon_entend_c(m, &cib[i], d)) continue;
        float k = cib[i].bruit / (d + 1);
        if (i == m->proie && m->etat == E_CHASSE) k *= 1.5f;   /* il prefere garder sa proie */
        if (k > bk) { bk = k; best = i; }
    }
    m->entendu = best >= 0;
    if (best >= 0) {
        const Cible *c = &cib[best];
        float d = dist_xz(m->pos, c->pos);
        if (m->etat == E_CHASSE && best == m->proie) { m->oubliT = 0; if ((m->repathT -= dt) <= 0) mon_viser(m, c->pos); }
        else if (d < 5.0f || c->bruit >= 0.9f) {          /* trop pres ou trop fort : il fonce, en hurlant */
            m->proie = best;
            mon_etat(m, E_CHASSE, 0); m->oubliT = 0; mon_viser(m, c->pos);
            play3d(S_RUGISSEMENT, m->pos, 1.3f, 0.9f + 0.15f * frand(), 25);
            sursauter(BPM_SURSAUT * 0.7f);
        } else if (m->etat != E_ENQUETE || dist_xz(m->cible, c->pos) > 3) {
            float err = d * 0.12f, a = frand() * 2 * PI;
            V3 v = v3(c->pos.x + sinf(a) * err, 0, c->pos.z + cosf(a) * err);
            if (cell_mur((int)floorf(v.x), (int)floorf(v.z))) v = c->pos;
            mon_etat(m, E_ENQUETE, 0); mon_viser(m, v);
        }
    } else if (bruitMondeForce > 0 && m->etat != E_CHASSE && dist_xz(m->pos, bruitMondePos) < RODEUR_OUIE * bruitMondeForce
               && dist_xz(m->cible, bruitMondePos) > 2) {
        mon_etat(m, E_ENQUETE, 0); mon_viser(m, bruitMondePos);   /* une alarme, quelque part */
    }
    switch (m->etat) {
    case E_ERRE: return RODEUR_ERRE_V;
    case E_ENQUETE: return RODEUR_ENQUETE_V;
    case E_CHASSE:
        if (!m->entendu && (m->oubliT += dt) > RODEUR_OUBLI) mon_etat(m, E_ENQUETE, 0);
        return RODEUR_CHASSE_V;
    case E_ECOUTE:
        if ((m->etatT -= dt) <= 0) mon_errer(m);
        return 0;
    }
    return 0;
}

/* ---- le Locataire ----------------------------------------------------------------------------- */
static int locataire_eclaire(const Monstre *m, float t) {
    if (dans_faisceau(m->pos)) return 1;
    if (lum_case(m->pos.x, m->pos.z) > LOCATAIRE_LUMIERE) return 1;
    for (int i = 0; i < nneons; i++)                      /* sous un neon fou allume */
        if (neon_dynamique(i) && neon_allume(i, t) && fabsf(neons[i].x - m->pos.x) < 1.6f && fabsf(neons[i].z - m->pos.z) < 1.6f) return 1;
    return 0;
}
static float locataire_tick(Monstre *m, float dt, float t) {
    if (m->vivT < LOCATAIRE_REVEIL) return 0;
    if (locataire_eclaire(m, t)) {
        if (m->etat != E_FIGE) mon_etat(m, E_FIGE, 0);
        return 0;
    }
    if (m->etat == E_FIGE) {                              /* le noir revient : il repart, et ca craque */
        play3d(S_CRAQUE, m->pos, 1.0f, 0.8f + 0.3f * frand(), 16);
        mon_etat(m, E_CHASSE, 0);
    }
    if (m->etat != E_CHASSE) mon_etat(m, E_CHASSE, 0);    /* il sait toujours ou vous etes */
    if ((m->repathT -= dt) <= 0) {
        float bd = 1e9f; int best = -1;                   /* le plus proche */
        for (int i = 0; i < MAXP; i++)
            if (cib[i].actif && !cib[i].mort && !cib[i].cache && dist_xz(m->pos, cib[i].pos) < bd) { bd = dist_xz(m->pos, cib[i].pos); best = i; }
        if (best >= 0) { m->proie = best; mon_viser(m, cib[best].pos); }
    }
    if ((m->sonT -= dt) <= 0) { play3d(S_CRAQUE, m->pos, 0.8f, 0.7f + 0.5f * frand(), 14); m->sonT = 0.6f + frand() * 0.9f; }
    return LOCATAIRE_V;
}

/* ---- la Rampante ------------------------------------------------------------------------------ */
static float rampante_tick(Monstre *m, float dt) {
    int voitK = -1; float bd = 1e9f;                      /* la lampe allumee la plus proche qu'elle voit */
    for (int i = 0; i < MAXP; i++) {
        const Cible *c = &cib[i];
        if (!c->actif || c->mort || !c->lampe) continue;
        float d = dist_xz(m->pos, c->pos);
        if (((d < RAMPANTE_VUE && visible_xz(m->pos.x, m->pos.z, c->pos.x, c->pos.z)) || d < RAMPANTE_PROCHE) && d < bd) { bd = d; voitK = i; }
    }
    int voit = voitK >= 0;
    if (voit) {
        if (m->etat != E_CHASSE) {                        /* elle a vu une lumiere */
            play3d(S_CRI_RAMPANTE, m->pos, 1.3f, 0.95f + 0.1f * frand(), 30);
            sursauter(BPM_SURSAUT);
            mon_etat(m, E_CHASSE, 0);
        }
        m->proie = voitK;
        m->oubliT = 0;
        if ((m->repathT -= dt) <= 0) mon_viser(m, cib[voitK].pos);
    }
    if ((m->sonT -= dt) <= 0) { play3d(S_GRATTE, m->pos, 0.9f, 0.9f + 0.3f * frand(), 13); m->sonT = m->etat == E_CHASSE ? 0.25f : 0.7f + frand(); }
    switch (m->etat) {
    case E_CHASSE:
        if (!voit && (m->oubliT += dt) > RAMPANTE_OUBLI) { mon_etat(m, E_ENQUETE, 0); }
        return RAMPANTE_CHASSE_V;
    case E_ENQUETE: return RAMPANTE_ERRE_V * 1.4f;
    case E_ECOUTE:
        if ((m->etatT -= dt) <= 0) mon_errer(m);
        return 0;
    default:                                              /* elle trotte par a-coups */
        return fmodf(m->vivT, 1.6f) < 0.9f ? RAMPANTE_ERRE_V : 0;
    }
}

static void monstres_tick(float dt, float t) {
    cibles_maj();
    if (!monstresEveilles) return;
    for (int i = 0; i < nmon; i++) {
        Monstre *m = &mon[i];
        if (!m->actif) continue;
        m->prevPos = m->pos;
        m->vivT += dt;
        float v = 0;
        if (m->bloqueT > 0) { m->bloqueT -= dt; continue; }   /* pris dans une machoire */
        if (m->type == MON_RODEUR) v = rodeur_tick(m, dt);
        else if (m->type == MON_LOCATAIRE) v = locataire_tick(m, dt, t);
        else v = rampante_tick(m, dt);
        if (m->etat != E_FIGE)
            for (int k = 0; k < MAXP; k++) {               /* il attrape ceux qu'il touche */
                if (!cib[k].actif || cib[k].mort || cib[k].cache || dist_xz(m->pos, cib[k].pos) >= RODEUR_PRISE) continue;   /* cache : il ne le trouve pas */
                if (k == slot_local()) { if (tueur < 0) tueur = i; }
                else reseau_mort_distante(k, m->type);
                cib[k].mort = 1;
            }
        if (v > 0 && mon_avancer(m, v * m->vitK, dt)) {    /* arrive */
            if (m->etat == E_CHASSE && cib[m->proie].actif) mon_viser(m, cib[m->proie].pos);
            else if (m->type == MON_RODEUR && (m->etat == E_ENQUETE || frand() < 0.5f)) {
                mon_etat(m, E_ECOUTE, 2.0f + frand() * 2.5f);
                play3d(S_RODEUR_CLIC, m->pos, 0.8f, 0.9f + 0.2f * frand(), 12);
            } else if (m->type == MON_RAMPANTE && frand() < 0.4f) mon_etat(m, E_ECOUTE, 1.0f + frand() * 2);
            else mon_errer(m);
        }
        if (m->etat == E_ERRE && (m->repathT -= dt) <= -8) mon_errer(m);   /* il change parfois d'idee */
        for (int k = 0; k < npieges; k++) {                /* les monstres aussi marchent dans les machoires */
            Piege *pg = &pieges[k];
            if (pg->type != PIEGE_MACHOIRE || !pg->arme || dist_xz(pg->pos, m->pos) > 0.4f) continue;
            pg->arme = 0; pg->t = 0; m->bloqueT = 4.0f;
            play3d(S_MACHOIRE, pg->pos, 1.2f, 0.9f, 25);
            play3d(m->type == MON_RAMPANTE ? S_CRI_RAMPANTE : S_RUGISSEMENT, m->pos, 1.2f, 1.2f, 25);
        }
        float moved = hlen(vsub(m->pos, m->prevPos));
        m->anim += moved * (m->type == MON_RAMPANTE ? 4.0f : 2.2f);
        m->pasDist += moved;
        if (m->type == MON_RODEUR && m->pasDist > 0.95f) { m->pasDist = 0; play3d(S_RODEUR_PAS, m->pos, 1.0f, 0.85f + 0.2f * frand(), 15); }
    }
    if (bruitMondeForce > 0) bruitMondeForce = fmaxf(0, bruitMondeForce - dt * 0.2f);
}

/* le monstre le plus proche du joueur (tension, coeur, lampe qui gresille) ; -1 s'il n'y en a pas */
static int mon_proche(float *d) {
    int best = -1; float bd = 1e9f;
    for (int i = 0; i < nmon; i++) {
        if (!mon[i].actif) continue;
        float dd = dist_xz(mon[i].pos, P.pos);            /* du joueur local (peur, coeur, lampe) */
        if (dd < bd) { bd = dd; best = i; }
    }
    *d = bd;
    return best;
}
