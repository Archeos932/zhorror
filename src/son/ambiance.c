/* BOURDONNEMENT - la musique : un moteur d'horreur synthetise en temps reel (inclus par son/audio.c, mixe dans
   audio_fill). Rien n'est ecrit d'avance : tout reagit a la tension (0..1) et au coeur du joueur.
     - nappe : quatre scies desaccordees en grappe (la, si bemol, re diese, mi : seconde mineure et triton),
       filtre qui s'ouvre avec la tension ; infra-basse qui ondule ;
     - coeur : chaque battement (amb_battement, cale sur le BPM du joueur) donne un "poum-poum" sourd ;
       en poursuite, un tambour grave tombe sur le meme battement ;
     - boite a musique : la melodie du centre commercial, desaccordee, bande qui pleure, notes qui sautent ;
       seulement quand c'est calme (c'est la qu'elle fait le plus peur) ;
     - cordes qui crissent : grappe aigue tremolo, au-dessus de 0,55 de tension ;
     - evenements : coups dans les tuyaux, chuchotements, a gauche ou a droite, au hasard ;
     - reverberation : deux filtres en peigne.
   Morceau de main.c (via audio.c) : ne se compile pas seul. */

static float ambTension;                                  /* 0..1, mise a jour par le jeu (screamers.c) */
static int ambActif, ambMort;                             /* en jeu ; screamer / ecran de mort (tout se tait) */
static float ambVol = 1.0f;

static double ambT;                                       /* temps (s) */
static float ambPadPh[4], ambPadLp[2], ambSubPh, ambTenL;  /* nappe */
static float ambCoeurT = 99, ambCoeurV, ambTambT = 99, ambTambV;   /* battement, tambour */
static float ambBoiteT, ambBoiteNoteT = 99, ambBoiteF, ambBoitePh, ambBoiteMod;   /* boite a musique */
static int ambBoiteI;
static float ambStrPh[4], ambStrLp;                       /* cordes */
static float ambEvT = 4, ambCoupT = 99, ambCoupF, ambCoupPan;   /* coup dans un tuyau */
static float ambChuT = 99, ambChuPan, ambChuBp, ambChuBpv;     /* chuchotement */
static float ambEnv = 0;                                  /* volume general (fondus) */
#define AMB_COMB1 1557
#define AMB_COMB2 2111
static float ambComb1[AMB_COMB1], ambComb2[AMB_COMB2];
static int ambC1, ambC2;

static unsigned ambRng = 2003;                            /* son propre hasard : ne touche pas a la graine du jeu */
static float amb_rand(void) { ambRng = ambRng * 1664525u + 1013904223u; return (ambRng >> 8) * (1.0f / 16777216.0f); }

static void amb_battement(float vol) {
    ambCoeurT = 0; ambCoeurV = vol;
    if (ambTension > 0.72f) { ambTambT = 0; ambTambV = (ambTension - 0.72f) / 0.28f; }
}

static float amb_saw(float ph) { return 2 * (ph - floorf(ph)) - 1; }

/* une image stereo de la musique */
static void amb_frame(float *outL, float *outR) {
    const float dt = 1.0f / SR;
    ambT += dt;
    float voulu = ambActif && !ambMort ? 1.0f : (ambActif ? 0.0f : 0.45f);
    ambEnv += (voulu - ambEnv) * (voulu > ambEnv ? 0.00004f : 0.0004f);
    ambTenL += (ambTension - ambTenL) * 0.00005f;          /* tension lissee (pas de saut) */
    float t = ambTenL;
    float L = 0, R = 0, envoi = 0;

    /* nappe : grappe dissonante, filtree par la tension */
    static const float pf[4] = { 55.0f, 58.27f, 77.78f, 82.41f };
    float wob = 1 + 0.004f * sinf((float)ambT * 0.31f);
    float a = 0, b = 0;
    for (int k = 0; k < 4; k++) {
        ambPadPh[k] += pf[k] * wob * (1 + 0.0015f * k) * dt;
        if (ambPadPh[k] > 1) ambPadPh[k] -= 1;
        if (k & 1) b += amb_saw(ambPadPh[k]); else a += amb_saw(ambPadPh[k]);
    }
    float fc = 120 + 1900 * t * t + 60 * sinf((float)ambT * 0.17f);
    float lk = 1 - expf(-2 * PI * fc * dt);
    ambPadLp[0] += lk * (a - ambPadLp[0]); ambPadLp[1] += lk * (b - ambPadLp[1]);
    float padV = 0.09f + 0.13f * t;
    L += ambPadLp[0] * padV; R += ambPadLp[1] * padV;
    ambSubPh += 30.87f * dt; if (ambSubPh > 1) ambSubPh -= 1;   /* infra-basse */
    float sub = sinf(ambSubPh * 2 * PI) * (0.16f + 0.1f * sinf((float)ambT * 0.09f));
    L += sub; R += sub;

    /* coeur : poum (0) - poum (0,17 s), grave et etouffe */
    if (ambCoeurT < 0.6f) {
        float c = 0, ct = ambCoeurT, c2 = ambCoeurT - 0.17f;
        c += sinf(2 * PI * (52 - 30 * ct) * ct) * expf(-ct * 24);
        if (c2 > 0) c += 0.75f * sinf(2 * PI * (46 - 26 * c2) * c2) * expf(-c2 * 26);
        c *= 0.75f * ambCoeurV;
        L += c; R += c;
        ambCoeurT += dt;
    }
    /* tambour de poursuite sur le battement */
    if (ambTambT < 0.8f) {
        float tt = ambTambT;
        float d = sinf(2 * PI * (85 - 45 * tt) * tt) * expf(-tt * 9) + (amb_rand() * 2 - 1) * 0.35f * expf(-tt * 30);
        d *= 0.55f * ambTambV;
        L += d; R += d; envoi += d * 0.3f;
        ambTambT += dt;
    }

    /* boite a musique : la melodie du centre commercial, au calme seulement */
    float calme = clampf(1 - t * 2.2f, 0, 1);
    if (calme > 0.01f) {
        static const signed char mel[16] = { 0, 3, 7, 12, 11, 7, 3, 2, 0, 3, 7, 10, 8, 7, 3, -1 };
        ambBoiteNoteT += dt;
        float pas = 1.15f + 0.4f * t;
        if (ambBoiteNoteT > pas) {
            ambBoiteNoteT = 0;
            if (amb_rand() > 0.12f) {                     /* une note sur huit saute */
                int n = mel[ambBoiteI % 16];
                float cents = (amb_rand() - 0.5f) * 50;   /* desaccordee */
                ambBoiteF = 440.0f * powf(2.0f, (n + cents / 100.0f) / 12.0f) * (ambBoiteI >= 16 ? 0.94f : 1.0f);
                ambBoiteT = 0; ambBoiteMod = 1;
            }
            ambBoiteI = (ambBoiteI + 1) % 32;
        }
        if (ambBoiteT < 2.5f) {
            float pl = 1 + 0.008f * sinf((float)ambT * 2 * PI * 0.6f);   /* la bande pleure */
            ambBoitePh += ambBoiteF * pl * dt;
            if (ambBoitePh > 1) ambBoitePh -= 1;
            ambBoiteMod *= 0.99985f;
            float s = sinf(2 * PI * ambBoitePh + 2.2f * ambBoiteMod * sinf(2 * PI * ambBoitePh * 3.5f));
            s *= expf(-ambBoiteT * 2.0f) * 0.07f * calme;
            L += s * 1.2f; R += s * 0.7f; envoi += s * 1.4f;
            ambBoiteT += dt;
        }
    }

    /* cordes qui crissent */
    if (t > 0.55f) {
        static const float sf[4] = { 1760, 1864.7f, 1975.5f, 2637 };
        float v = (t - 0.55f) / 0.45f; v = v * v * 0.075f;
        float trem = 0.6f + 0.4f * sinf((float)ambT * 2 * PI * 9.0f);
        float s = 0;
        for (int k = 0; k < 4; k++) {
            ambStrPh[k] += sf[k] * (1 + 0.004f * sinf((float)ambT * (5 + k))) * dt;
            if (ambStrPh[k] > 1) ambStrPh[k] -= 1;
            s += amb_saw(ambStrPh[k]);
        }
        ambStrLp += 0.35f * (s - ambStrLp);
        s = (ambStrLp + (amb_rand() * 2 - 1) * 0.4f) * v * trem;
        L += s * 0.8f; R += s; envoi += s;
    }

    /* evenements au hasard : coups dans les tuyaux, chuchotements */
    ambEvT -= dt;
    if (ambEvT <= 0 && ambActif) {
        ambEvT = 7 + amb_rand() * 14 - 4 * t;
        if (amb_rand() < 0.6f) { ambCoupT = 0; ambCoupF = 0.7f + amb_rand() * 0.8f; ambCoupPan = amb_rand(); }
        else { ambChuT = 0; ambChuPan = amb_rand(); }
    }
    if (ambCoupT < 1.5f) {
        float ct = ambCoupT, f = ambCoupF;
        float s = (sinf(2 * PI * 183 * f * ct) * 0.5f + sinf(2 * PI * 431 * f * ct) * 0.3f + sinf(2 * PI * 1023 * f * ct) * 0.15f)
                * expf(-ct * 5) * 0.22f;
        L += s * (1 - ambCoupPan); R += s * ambCoupPan; envoi += s * 1.5f;
        ambCoupT += dt;
    }
    if (ambChuT < 1.8f) {
        float ct = ambChuT;
        float fcb = 900 + 1400 * sinf(ct * 2.6f) * sinf(ct * 2.6f);   /* formants qui glissent : on croit entendre des mots */
        float f = 2 * sinf(PI * fcb * dt), q = 0.18f;
        float hp = (amb_rand() * 2 - 1) - ambChuBp - q * ambChuBpv;
        ambChuBpv += f * hp; ambChuBp += f * ambChuBpv;
        float env = sinf(PI * ct / 1.8f) * (0.6f + 0.4f * sinf(ct * 23));
        float s = ambChuBpv * env * 0.09f;
        L += s * (1 - ambChuPan) * 1.4f; R += s * ambChuPan * 1.4f; envoi += s;
        ambChuT += dt;
    }

    /* reverberation : deux peignes */
    float r1 = ambComb1[ambC1], r2 = ambComb2[ambC2];
    ambComb1[ambC1] = envoi + r1 * 0.78f; ambComb2[ambC2] = envoi + r2 * 0.74f;
    ambC1 = (ambC1 + 1) % AMB_COMB1; ambC2 = (ambC2 + 1) % AMB_COMB2;
    L += (r1 * 0.6f + r2 * 0.4f) * 0.5f; R += (r1 * 0.4f + r2 * 0.6f) * 0.5f;

    float g = ambEnv * ambVol;
    *outL = L * g; *outR = R * g;
}
