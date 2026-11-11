/* BOURDONNEMENT - synthese des bruitages (une fois, au demarrage)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static float noise(void) { return frand() * 2 - 1; }
static float *sbuf(float secs, int *n) { *n = (int)(secs * SR); return (float *)calloc(*n, sizeof(float)); }
static Sound mksound(float *f, int n) {
    Sound s; s.len = n; s.data = (short *)malloc(n * sizeof(short));
    for (int i = 0; i < n; i++) s.data[i] = (short)(clampf(f[i], -1, 1) * 32000.0f);
    free(f);
    return s;
}

static void make_sounds(void) {
    int n; float *b, t, lp, lp2;
    unsigned int keep = rng;                              /* les sons ne changent pas la graine du jeu */
    rng = 4242;

    /* bourdonnement des neons : 2 s exactement = 240 periodes de 120 Hz, donc la boucle se raccorde sans clic.
       Fondamentale faible, harmoniques impaires (le ballast), un peu de souffle filtre module a 120 Hz. */
    b = sbuf(2.0f, &n); lp = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        float w = 2 * PI * 120 * t;
        float h = 0.30f * sinf(w) + 0.22f * sinf(3 * w) + 0.12f * sinf(5 * w + 0.4f) + 0.06f * sinf(7 * w + 1.1f)
                + 0.08f * sinf(2 * w);
        lp += 0.15f * (noise() - lp);
        float buzz = lp * 0.10f * (0.5f + 0.5f * sinf(w));
        b[i] = (h + buzz) * 0.8f;
    }
    {                                                     /* le souffle ne boucle pas : fondu croise sur 20 ms */
        int x = (int)(0.02f * SR);
        for (int i = 0; i < x; i++) { float k = (float)i / x; b[i] = b[i] * k + b[n - x + i] * (1 - k); }
        b[n - 1] = b[0];
    }
    sounds[S_BOURDON] = mksound(b, n);

    /* gresillement : claquements secs et irreguliers + ronflement qui saute */
    b = sbuf(0.45f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        float gate = (sinf(t * 2 * PI * 23) + sinf(t * 2 * PI * 37 + 1) > 0.6f) ? 1.0f : 0.15f;
        float crack = frand() < 0.004f ? noise() * 2.5f : 0;
        b[i] = (sinf(t * 2 * PI * 120) * 0.4f + noise() * 0.25f) * gate * expf(-t * 4) + crack * expf(-t * 3);
    }
    sounds[S_GRESILLE] = mksound(b, n);

    /* pas sur moquette mouillee : choc sourd + petit bruit d'eau (squish) */
    b = sbuf(0.26f, &n); lp = 0; lp2 = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        lp += 0.04f * (noise() - lp);
        lp2 += 0.25f * (noise() - lp2);
        float squish = t > 0.03f ? lp2 * 0.35f * expf(-(t - 0.03f) * 30) * (0.5f + 0.5f * sinf(t * 2 * PI * 340)) : 0;
        b[i] = lp * 3.0f * expf(-t * 35) + sinf(t * 2 * PI * 60) * 0.35f * expf(-t * 30) + squish;
    }
    sounds[S_PAS] = mksound(b, n);

    /* souffle : expiration (bruit filtre, enveloppe en cloche) */
    b = sbuf(0.7f, &n); lp = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        lp += 0.12f * (noise() - lp);
        float env = sinf(PI * t / 0.7f);
        b[i] = lp * 1.4f * env * env;
    }
    sounds[S_SOUFFLE] = mksound(b, n);

    /* clic de menu : petit relais electrique */
    b = sbuf(0.06f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        b[i] = (noise() * 0.5f + sinf(t * 2 * PI * 1800) * 0.3f) * expf(-t * 90);
    }
    sounds[S_CLIC] = mksound(b, n);

    /* lampe : deux petits claquements secs (interrupteur) */
    b = sbuf(0.08f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        b[i] = noise() * 0.7f * (expf(-t * 400) + (t > 0.03f ? 0.6f * expf(-(t - 0.03f) * 500) : 0));
    }
    sounds[S_LAMPE] = mksound(b, n);

    /* fusible : tintement metallique (partiels inharmoniques) */
    b = sbuf(0.6f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        b[i] = (sinf(t * 2 * PI * 1210) * 0.35f + sinf(t * 2 * PI * 1873) * 0.25f + sinf(t * 2 * PI * 2954) * 0.18f) * expf(-t * 7)
             + noise() * 0.4f * expf(-t * 200);
    }
    sounds[S_FUSIBLE] = mksound(b, n);

    /* ding d'ascenseur : cloche un peu fausse (le jeu ment jusque dans les ascenseurs) */
    b = sbuf(2.0f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        float d1 = sinf(t * 2 * PI * 880) + 0.5f * sinf(t * 2 * PI * 2428) + 0.3f * sinf(t * 2 * PI * 1761);
        float d2 = t > 0.35f ? (sinf((t - 0.35f) * 2 * PI * 698) + 0.4f * sinf((t - 0.35f) * 2 * PI * 1931)) * expf(-(t - 0.35f) * 2.2f) : 0;
        b[i] = (d1 * expf(-t * 2.5f) + d2) * 0.28f;
    }
    sounds[S_DING] = mksound(b, n);

    /* portes : grondement et raclement metallique */
    b = sbuf(1.4f, &n); lp = 0; lp2 = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        lp += 0.02f * (noise() - lp);
        lp2 += 0.3f * (noise() - lp2);
        float env = sinf(PI * t / 1.4f);
        float scrape = lp2 * 0.25f * (0.5f + 0.5f * sinf(t * 2 * PI * (7 + 3 * sinf(t * 5))));
        b[i] = (lp * 4.0f + sinf(t * 2 * PI * 48) * 0.3f + scrape) * env;
    }
    sounds[S_PORTE] = mksound(b, n);

    /* coeur : poum-poum sourd */
    b = sbuf(0.5f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        float a1 = expf(-t * 28), t2 = t - 0.19f, a2 = t2 > 0 ? 0.7f * expf(-t2 * 30) : 0;
        b[i] = sinf(t * 2 * PI * (58 - 20 * t)) * (a1 + a2) * 0.9f;
    }
    sounds[S_COEUR] = mksound(b, n);

    /* cri du joueur : voix rauque (scie filtree + souffle), hauteur qui retombe */
    b = sbuf(0.9f, &n); lp = 0;
    {
        float ph = 0;
        for (int i = 0; i < n; i++) {
            t = (float)i / SR;
            ph += (230 - 60 * t) / SR;
            float saw = 2 * (ph - floorf(ph)) - 1;
            lp += 0.18f * (saw - lp);
            float env = fminf(1, t * 20) * expf(-t * 2.2f);
            b[i] = (lp * 1.4f + noise() * 0.12f) * env;
        }
    }
    sounds[S_CRI] = mksound(b, n);

    /* drone de tension : 4 s exactement, frequences a un nombre entier de periodes (boucle sans clic) */
    b = sbuf(4.0f, &n); lp = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        lp += 0.01f * (noise() - lp);
        b[i] = (sinf(t * 2 * PI * 41.25f) * 0.35f + sinf(t * 2 * PI * 43.75f) * 0.3f + sinf(t * 2 * PI * 61.75f) * 0.15f
              + sinf(t * 2 * PI * 82.5f) * 0.1f) * 0.9f + lp * 1.5f;
    }
    {
        int x = (int)(0.05f * SR);
        for (int i = 0; i < x; i++) { float k = (float)i / x; b[i] = b[i] * k + b[n - x + i] * (1 - k); }
        b[n - 1] = b[0];
    }
    sounds[S_DRONE] = mksound(b, n);

    /* pas du Rodeur : impact lourd, mouille, puis la jambe qu'on traine */
    b = sbuf(0.75f, &n); lp = 0; lp2 = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        lp += 0.03f * (noise() - lp);
        lp2 += 0.2f * (noise() - lp2);
        float drag = t > 0.25f ? lp2 * 0.25f * sinf(PI * (t - 0.25f) / 0.5f) : 0;
        b[i] = lp * 4.5f * expf(-t * 18) + sinf(t * 2 * PI * 42) * 0.7f * expf(-t * 14) + drag;
    }
    sounds[S_RODEUR_PAS] = mksound(b, n);

    /* souffle du Rodeur : 3 s = deux respirations rauques (enveloppe nulle aux bords : boucle sans clic) */
    b = sbuf(3.0f, &n);
    {
        float bp = 0, bpv = 0, ph = 0;                    /* filtre passe-bande resonant (variable d'etat) */
        for (int i = 0; i < n; i++) {
            t = (float)i / SR;
            float fc = 380 + 160 * sinf(t * 2 * PI * 0.66f);
            float f = 2 * sinf(PI * fc / SR), q = 0.22f;
            float in = noise();
            float hp = in - bp - q * bpv;
            bpv += f * hp; bp += f * bpv;
            ph += (68 + 6 * sinf(t * 9)) / SR;
            float growl = (2 * (ph - floorf(ph)) - 1) * 0.25f;
            float env = powf(fabsf(sinf(PI * t / 1.5f)), 1.5f);
            b[i] = (bpv * 0.55f + growl * (0.5f + 0.5f * sinf(t * 2 * PI * 11))) * env;
        }
    }
    sounds[S_RODEUR_SOUFFLE] = mksound(b, n);

    /* claquements du Rodeur (il ecoute) : salve irreguliere de clics aigus */
    b = sbuf(1.3f, &n);
    {
        float tc = 0.02f;
        for (int c = 0; c < 9 && tc < 1.2f; c++) {
            int i0 = (int)(tc * SR);
            for (int i = 0; i < (int)(0.02f * SR) && i0 + i < n; i++) {
                float tt = (float)i / SR;
                b[i0 + i] += (sinf(tt * 2 * PI * (2400 + c * 90)) * 0.6f + noise() * 0.3f) * expf(-tt * 300);
            }
            tc += 0.06f + frand() * 0.12f;
        }
    }
    sounds[S_RODEUR_CLIC] = mksound(b, n);

    /* screamer de mort : grappe de scies dissonantes, vibrato fou, hauteur qui monte puis s'effondre, sature */
    b = sbuf(1.7f, &n);
    {
        static const float fq[6] = { 311, 329, 466, 659, 932, 1397 };
        float ph[6] = { 0 };
        for (int i = 0; i < n; i++) {
            t = (float)i / SR;
            float bend = t < 0.4f ? 1 + t * 0.6f : 1.24f * expf(-(t - 0.4f) * 0.9f);
            float acc = 0;
            for (int k = 0; k < 6; k++) {
                ph[k] += fq[k] * bend * (1 + 0.03f * sinf(t * 2 * PI * (13 + k * 3))) / SR;
                acc += 2 * (ph[k] - floorf(ph[k])) - 1;
            }
            float env = fminf(1, t * 200) * (t < 1.1f ? 1 : expf(-(t - 1.1f) * 5));
            b[i] = tanhf((acc * 0.35f + noise() * 0.6f) * 2.5f) * env * 0.95f;
        }
    }
    sounds[S_HURLEMENT] = mksound(b, n);

    /* stinger (apparition) : cordes aigues qui crissent + coup sourd */
    b = sbuf(1.1f, &n);
    {
        float ph[4] = { 0 };
        static const float fq[4] = { 1760, 1865, 2489, 2637 };
        for (int i = 0; i < n; i++) {
            t = (float)i / SR;
            float acc = 0;
            for (int k = 0; k < 4; k++) {
                ph[k] += fq[k] * (1 + 0.012f * sinf(t * 2 * PI * 31)) / SR;
                acc += 2 * (ph[k] - floorf(ph[k])) - 1;
            }
            float env = fminf(1, t * 60) * expf(-t * 2.4f);
            b[i] = (acc * 0.2f * (0.6f + 0.4f * sinf(t * 2 * PI * 18)) + noise() * 0.15f) * env
                 + sinf(t * 2 * PI * 50) * 0.8f * expf(-t * 9);
        }
    }
    sounds[S_STINGER] = mksound(b, n);

    /* faux bruit : un choc lourd quelque part, avec sa queue d'echo */
    b = sbuf(1.6f, &n); lp = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        lp += 0.025f * (noise() - lp);
        float boom = lp * 5 * expf(-t * 12) + sinf(t * 2 * PI * 55) * 0.6f * expf(-t * 10);
        b[i] = boom;
        int j = i - (int)(0.21f * SR), k = i - (int)(0.47f * SR);
        if (j >= 0) b[i] += b[j] * 0.35f;
        if (k >= 0) b[i] += b[k] * 0.2f;
    }
    sounds[S_FAUX_BRUIT] = mksound(b, n);

    rng = keep;
}
