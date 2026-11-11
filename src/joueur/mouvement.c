/* BOURDONNEMENT - le joueur : marche, course et endurance, accroupi, balancement de tete, pas
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   Axe Y vertical ; yaw = 0 regarde vers -Z, yaw croissant tourne a droite (comme VELOCITY). */

typedef struct {
    V3 pos, prevPos, vel;                                 /* pos : aux pieds */
    float yaw, pitch;
    float endurance;                                      /* secondes de course restantes */
    int essouffle;                                        /* a bout de souffle : plus de course jusqu'a ESSOUFFLE_SEUIL */
    float oeil, prevOeil;                                 /* hauteur des yeux (lissee entre debout et accroupi) */
    float pasDist;                                        /* distance depuis le dernier pas */
    float bob, prevBob;                                   /* phase du balancement de tete */
    int pas;                                              /* nombre de pas (tests) */
    int dead;
} Player;
static Player P;

static inline V3 view_dir(float yaw, float pitch) {
    return v3(sinf(yaw) * cosf(pitch), sinf(pitch), -cosf(yaw) * cosf(pitch));
}

static void player_reset(void) {
    memset(&P, 0, sizeof P);
    P.pos = P.prevPos = v3(0, 0, 7.5f);
    P.endurance = ENDURANCE_MAX;
    P.oeil = P.prevOeil = OEIL_DEBOUT;
}

/* un pas de simulation : lit act[] (jamais le clavier) */
static void player_tick(float dt) {
    P.prevPos = P.pos; P.prevOeil = P.oeil; P.prevBob = P.bob;
    float fx = sinf(P.yaw), fz = -cosf(P.yaw);            /* avant, a plat */
    float rx = cosf(P.yaw), rz = sinf(P.yaw);             /* droite */
    float ix = (float)act[A_DROITE] - act[A_GAUCHE], iz = (float)act[A_AVANCER] - act[A_RECULER];
    float il = sqrtf(ix * ix + iz * iz);
    if (il > 1) { ix /= il; iz /= il; }

    int accroupi = act[A_ACCROUPI];
    int court = act[A_COURIR] && !accroupi && !P.essouffle && iz > 0.1f && P.endurance > 0;
    float vmax = accroupi ? ACCROUPI_V : court ? COURSE_V : MARCHE_V;
    if (court) {
        P.endurance -= dt;
        if (P.endurance <= 0) { P.endurance = 0; P.essouffle = 1; play(S_SOUFFLE, 0.5f, 1.0f); }
    } else {
        P.endurance = fminf(ENDURANCE_MAX, P.endurance + dt * ENDURANCE_MAX / ENDURANCE_RECUP);
        if (P.essouffle && P.endurance >= ESSOUFFLE_SEUIL * ENDURANCE_MAX) P.essouffle = 0;
    }

    V3 want = v3((fx * iz + rx * ix) * vmax, 0, (fz * iz + rz * ix) * vmax);
    float k = fminf(1, ACCEL_K * dt);
    P.vel = vadd(P.vel, vmul(vsub(want, P.vel), k));
    P.pos = vadd(P.pos, vmul(P.vel, dt));
    salle_collision(&P.pos.x, &P.pos.z, RAYON_JOUEUR);

    float target = accroupi ? OEIL_ACCROUPI : OEIL_DEBOUT;
    P.oeil += (target - P.oeil) * fminf(1, OEIL_K * dt);

    /* pas et balancement : une demi-periode de balancement = un pas */
    float moved = hlen(vsub(P.pos, P.prevPos));
    float pasD = court ? PAS_DIST_COURSE : PAS_DIST_MARCHE;
    P.pasDist += moved;
    P.bob += moved / pasD * PI;
    if (P.pasDist >= pasD) {
        P.pasDist -= pasD;
        P.pas++;
        float spd = hlen(P.vel);
        play(S_PAS, PAS_VOL * clampf(spd / COURSE_V, 0.25f, 1.0f) * (accroupi ? 0.35f : 1.0f), 0.9f + 0.2f * frand());
    }
}

/* hauteur des yeux interpolee, balancement compris (rendu) */
static float player_eye_y(float alpha) {
    float oeil = P.prevOeil + (P.oeil - P.prevOeil) * alpha;
    float bob = P.prevBob + (P.bob - P.prevBob) * alpha;
    float amp = BOB_AMP * clampf(hlen(P.vel) / COURSE_V, 0, 1);
    return oeil - amp * fabsf(sinf(bob));
}
