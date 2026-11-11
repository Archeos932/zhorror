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

    rng = keep;
}
