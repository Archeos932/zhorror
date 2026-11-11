/* BOURDONNEMENT - le joueur : marche, course et endurance, accroupi, se pencher, balancement de tete, pas,
   bruit emis (ce que le monstre entend), lampe torche et batterie.
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   Axe Y vertical ; yaw = 0 regarde vers -Z, yaw croissant tourne a droite (comme VELOCITY). */

typedef struct {
    V3 pos, prevPos, vel;                                 /* pos : aux pieds */
    float yaw, pitch;
    float endurance;                                      /* secondes de course restantes */
    int essouffle;                                        /* a bout de souffle : plus de course jusqu'a ESSOUFFLE_SEUIL */
    int court, accroupi;                                  /* etat du tick (rendu, bruit) */
    float oeil, prevOeil;                                 /* hauteur des yeux (lissee entre debout et accroupi) */
    float penche, prevPenche;                             /* -1 gauche .. 1 droite (lisse) */
    float roulis, prevRoulis;                             /* inclinaison laterale en marchant (degres) */
    float courseK;                                        /* 0..1 lisse : champ de vision de la course */
    float pasDist;                                        /* distance depuis le dernier pas */
    float bob, prevBob;                                   /* phase du balancement de tete */
    int pas;                                              /* nombre de pas (tests) */
    float bruit;                                          /* bruit emis maintenant (0..2) */
    float bruitImpulse;                                   /* bruits ponctuels (lampe, ramasser, cri), retombent vite */
    int lampe;                                            /* lampe allumee */
    float batterie;                                       /* secondes restantes */
    float lampeYaw, lampePitch;                           /* le faisceau suit le regard avec un retard */
    float souffleT;                                       /* prochaine respiration audible (essouffle) */
    float piegeT;                                         /* > 0 : pris dans une machoire de loup */
    int dead;
} Player;
static Player P;

static inline V3 view_dir(float yaw, float pitch) {
    return v3(sinf(yaw) * cosf(pitch), sinf(pitch), -cosf(yaw) * cosf(pitch));
}

static void player_reset(void) {
    memset(&P, 0, sizeof P);
    P.pos = P.prevPos = carteDepart;
    P.yaw = P.lampeYaw = carteDepartYaw;
    P.endurance = ENDURANCE_MAX;
    P.oeil = P.prevOeil = OEIL_DEBOUT;
    P.lampe = 1;
    P.batterie = LAMPE_BATTERIE;
}

static void faire_bruit(float b) { if (b > P.bruitImpulse) P.bruitImpulse = b; }

/* sursaut : fait bondir le coeur (lu par jeu/screamers.c) */
static float sursautDemande;
static void sursauter(float bpm) { if (bpm > sursautDemande) sursautDemande = bpm; }

/* bruit pose dans le monde (alarme d'un faux fusible...) : les monstres qui ecoutent l'entendent */
static V3 bruitMondePos;
static float bruitMondeForce;                             /* 0 = rien en ce moment */
static void bruit_monde(V3 p, float f) { if (f >= bruitMondeForce) { bruitMondePos = p; bruitMondeForce = f; } }

/* un pas de simulation : lit act[] (jamais le clavier) */
static void player_tick(float dt) {
    P.prevPos = P.pos; P.prevOeil = P.oeil; P.prevBob = P.bob; P.prevPenche = P.penche; P.prevRoulis = P.roulis;
    if (P.dead) { P.vel = v3(0, 0, 0); return; }
    float fx = sinf(P.yaw), fz = -cosf(P.yaw);            /* avant, a plat */
    float rx = cosf(P.yaw), rz = sinf(P.yaw);             /* droite */
    float ix = (float)act[A_DROITE] - act[A_GAUCHE], iz = (float)act[A_AVANCER] - act[A_RECULER];
    float il = sqrtf(ix * ix + iz * iz);
    if (il > 1) { ix /= il; iz /= il; }

    P.accroupi = act[A_ACCROUPI];
    P.court = act[A_COURIR] && !P.accroupi && !P.essouffle && iz > 0.1f && P.endurance > 0;
    float vmax = P.accroupi ? ACCROUPI_V : P.court ? COURSE_V : MARCHE_V;
    int solIci = sol_en(P.pos.x, P.pos.z);
    if (solIci == SOL_EAU) vmax *= EAU_FREIN;
    if (P.piegeT > 0) {                                   /* pris au piege : on ne bouge plus, on se debat (E) */
        P.piegeT -= dt;
        if (actP[A_UTILISER]) { P.piegeT -= MACHOIRE_E; play(S_MACHOIRE, 0.25f, 1.4f + 0.2f * frand()); faire_bruit(0.6f); }
        vmax = 0; il = 0; ix = iz = 0;
        P.court = 0;
    }
    if (P.court) {
        P.endurance -= dt;
        if (P.endurance <= 0) { P.endurance = 0; P.essouffle = 1; P.souffleT = 0; }
    } else {
        P.endurance = fminf(ENDURANCE_MAX, P.endurance + dt * ENDURANCE_MAX / ENDURANCE_RECUP);
        if (P.essouffle && P.endurance >= ESSOUFFLE_SEUIL * ENDURANCE_MAX) P.essouffle = 0;
    }
    if (P.essouffle && (P.souffleT -= dt) <= 0) { play(S_SOUFFLE, 0.45f, 0.9f + 0.2f * frand()); P.souffleT = 0.9f; }

    V3 want = v3((fx * iz + rx * ix) * vmax, 0, (fz * iz + rz * ix) * vmax);
    float k = fminf(1, (il > 0 ? ACCEL_K : FREIN_K) * dt);
    P.vel = vadd(P.vel, vmul(vsub(want, P.vel), k));
    P.pos = vadd(P.pos, vmul(P.vel, dt));
    carte_collision(&P.pos.x, &P.pos.z, RAYON_JOUEUR);
    if (dt > 0) P.vel = vmul(vsub(P.pos, P.prevPos), 1.0f / dt);   /* contre un mur, la vitesse tombe vraiment */
    P.vel.y = 0;

    float target = P.accroupi ? OEIL_ACCROUPI : OEIL_DEBOUT;
    P.oeil += (target - P.oeil) * fminf(1, OEIL_K * dt);
    float pw = (float)act[A_PENCHER_D] - act[A_PENCHER_G];
    if (hlen(P.vel) > MARCHE_V * 1.1f) pw = 0;            /* on ne se penche pas en courant */
    P.penche += (pw - P.penche) * fminf(1, PENCHE_K * dt);
    float lat = (P.vel.x * rx + P.vel.z * rz) / COURSE_V;   /* vitesse laterale -> petit roulis */
    P.roulis += (-lat * ROULIS_STRAFE - P.roulis) * fminf(1, 6 * dt);
    P.courseK += ((P.court ? 1.0f : 0.0f) - P.courseK) * fminf(1, 4 * dt);

    /* pas et balancement : une demi-periode de balancement = un pas */
    float moved = hlen(vsub(P.pos, P.prevPos)), spd = hlen(P.vel);
    float pasD = P.court ? PAS_DIST_COURSE : PAS_DIST_MARCHE;
    P.pasDist += moved;
    P.bob += moved / pasD * PI;
    if (P.pasDist >= pasD) {
        P.pasDist -= pasD;
        P.pas++;
        float pv = PAS_VOL * clampf(spd / COURSE_V, 0.25f, 1.0f) * (P.accroupi ? 0.3f : 1.0f), k2 = P.accroupi ? 0.4f : 1.0f;
        if (solIci == SOL_EAU) { play(S_FLAQUE, pv * 1.6f + 0.15f, 0.85f + 0.3f * frand()); faire_bruit(BRUIT_EAU * k2); }
        else if (solIci == SOL_VERRE) { play(S_VERRE, pv * 1.6f + 0.2f, 0.85f + 0.3f * frand()); faire_bruit(BRUIT_VERRE * k2); }
        else play(S_PAS, pv, 0.9f + 0.2f * frand());
    }

    /* lampe */
    if (actP[A_LAMPE]) {
        if (P.batterie > 0 || P.lampe) { P.lampe = !P.lampe; play(S_LAMPE, 0.5f, P.lampe ? 1.1f : 0.9f); faire_bruit(BRUIT_LAMPE); }
    }
    if (P.lampe) { P.batterie -= dt; if (P.batterie <= 0) { P.batterie = 0; P.lampe = 0; play(S_LAMPE, 0.4f, 0.7f); } }
    float lk = fminf(1, LAMPE_RETARD_K * dt);
    float dy = P.yaw - P.lampeYaw;
    while (dy > PI) dy -= 2 * PI;
    while (dy < -PI) dy += 2 * PI;
    P.lampeYaw += dy * lk; P.lampePitch += (P.pitch - P.lampePitch) * lk;

    if (actP[A_CRIER]) { play(S_CRI, 0.8f, 0.95f + 0.1f * frand()); faire_bruit(BRUIT_CRI); }

    /* bruit : selon l'allure (proportionnel a la vitesse), plus les impulsions */
    float b = 0;
    if (spd > 0.15f) b = P.accroupi ? BRUIT_ACCROUPI : P.court ? BRUIT_COURSE : BRUIT_MARCHE * clampf(spd / MARCHE_V, 0.3f, 1.0f);
    if (P.essouffle) b = fmaxf(b, BRUIT_SOUFFLE);
    P.bruitImpulse = fmaxf(0, P.bruitImpulse - BRUIT_DECROIT * dt);
    P.bruit = fmaxf(b, P.bruitImpulse);
}

/* position des yeux interpolee : accroupi, balancement, penche (sans traverser le mur) */
static V3 player_eye(float alpha, float *roll) {
    V3 p = vlerp(P.prevPos, P.pos, alpha);
    float oeil = P.prevOeil + (P.oeil - P.prevOeil) * alpha;
    float bob = P.prevBob + (P.bob - P.prevBob) * alpha;
    float penche = P.prevPenche + (P.penche - P.prevPenche) * alpha;
    float amp = clampf(hlen(P.vel) / COURSE_V, 0, 1);
    float rx = cosf(P.yaw), rz = sinf(P.yaw);
    float lat = BOB_LAT * amp * sinf(bob) + penche * PENCHE_DIST;
    if (fabsf(lat) > 0.001f) {                            /* ne pas mettre la tete dans le mur */
        float s = lat > 0 ? 1.0f : -1.0f;
        float libre = ray_grille(p.x, p.z, rx * s, rz * s, fabsf(lat) + 0.15f) - 0.15f;
        lat = s * clampf(libre, 0, fabsf(lat));
    }
    p.x += rx * lat; p.z += rz * lat;
    p.y = oeil - BOB_AMP * amp * fabsf(sinf(bob));
    *roll = penche * PENCHE_ROULIS + (P.prevRoulis + (P.roulis - P.prevRoulis) * alpha);
    return p;
}
