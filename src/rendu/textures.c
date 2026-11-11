/* BOURDONNEMENT - textures fabriquees au demarrage (aucun fichier image) : papier peint jaune, moquette humide,
   dalles de plafond, diffuseur de neon, faisceau de lampe, acier brosse.
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   Bruit de valeur periodique : chaque texture se repete sans couture. Mipmaps faits main (repris de VELOCITY). */

enum { MAT_MOQUETTE, MAT_PAPIER, MAT_PLAFOND, MAT_PLINTHE, MAT_CORNICHE, MAT_CLOISON, MAT_COUNT };
static GLuint texMat[MAT_COUNT], texNeon, texLampe, texMetal;
static const float matEchelle[MAT_COUNT] = { 0.5f, 0.5f, 1.0f / 2.4f, 1.0f, 1.0f, 0.5f };   /* repetitions par metre */

static float tex_h(int x, int y, unsigned s) {
    return (float)(hash32((unsigned)x * 73856093u ^ (unsigned)y * 19349663u ^ s) & 0xffff) / 65535.0f;
}
/* bruit de valeur lisse, periode per (en cellules) */
static float vnoise(float x, float y, int per, unsigned s) {
    int x0 = (int)floorf(x), y0 = (int)floorf(y);
    float fx = x - x0, fy = y - y0;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    int xa = ((x0 % per) + per) % per, ya = ((y0 % per) + per) % per, xb = (xa + 1) % per, yb = (ya + 1) % per;
    float a = tex_h(xa, ya, s), b = tex_h(xb, ya, s), c = tex_h(xa, yb, s), d = tex_h(xb, yb, s);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}
/* somme d'octaves ; u, v dans [0, 1) */
static float fbm(float u, float v, int per, int oct, unsigned s) {
    float r = 0, amp = 0.5f, tot = 0;
    for (int o = 0; o < oct; o++) {
        r += amp * vnoise(u * per, v * per, per, s + (unsigned)o * 101u);
        tot += amp; amp *= 0.5f; per *= 2;
    }
    return r / tot;
}

static void upload_rgb_mip(GLuint *tex, unsigned char *img, int N) {
    glGenTextures(1, tex);
    glBindTexture(GL_TEXTURE_2D, *tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    for (int level = 0, s = N; ; level++) {
        glTexImage2D(GL_TEXTURE_2D, level, GL_RGB, s, s, 0, GL_RGB, GL_UNSIGNED_BYTE, img);
        if (s == 1) break;
        int h = s / 2;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < h; x++)
                for (int c = 0; c < 3; c++) {
                    int sum = img[((2 * y) * s + 2 * x) * 3 + c] + img[((2 * y) * s + 2 * x + 1) * 3 + c] +
                              img[((2 * y + 1) * s + 2 * x) * 3 + c] + img[((2 * y + 1) * s + 2 * x + 1) * 3 + c];
                    img[(y * h + x) * 3 + c] = (unsigned char)(sum / 4);
                }
        s = h;
    }
}

static void px_set(unsigned char *img, int N, int x, int y, float r, float g, float b) {
    unsigned char *p = img + ((size_t)y * N + x) * 3;
    p[0] = (unsigned char)(clampf(r, 0, 1) * 255); p[1] = (unsigned char)(clampf(g, 0, 1) * 255); p[2] = (unsigned char)(clampf(b, 0, 1) * 255);
}

static void make_textures(void) {
    const int N = 256;
    unsigned char *img = (unsigned char *)malloc((size_t)N * N * 3);

    /* papier peint (2 m) : jaune sale, bandes verticales, motif de losanges presque efface, taches d'humidite */
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float u = (float)x / N, v = (float)y / N;
            float k = 1.0f;
            float bande = fmodf(u * 12, 1.0f);
            if (bande < 0.025f || bande > 0.975f) k *= 0.88f;              /* jointure des les */
            float du = fabsf(fmodf(u * 24, 1.0f) - 0.5f), dv = fabsf(fmodf(v * 16, 1.0f) - 0.5f);
            float los = du + dv;
            if (los > 0.44f && los < 0.5f) k *= 0.95f;                     /* losanges */
            if (los < 0.08f) k *= 1.03f;
            k *= 0.94f + 0.12f * fbm(u, v, 4, 3, 11);                       /* marbrure */
            float tache = fbm(u, v, 3, 4, 23);
            float tr = 1, tg = 1, tb = 1;
            if (tache > 0.62f) { float t = clampf((tache - 0.62f) * 5, 0, 1); tr = 1 - 0.16f * t; tg = 1 - 0.2f * t; tb = 1 - 0.3f * t; }
            k *= 0.97f + 0.06f * tex_h(x, y, 5);                            /* fibres du papier */
            px_set(img, N, x, y, 0.80f * k * tr, 0.72f * k * tg, 0.42f * k * tb);
        }
    upload_rgb_mip(&texMat[MAT_PAPIER], img, N);

    /* moquette (2 m) : fibres, touffes, grandes zones mouillees plus sombres */
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float u = (float)x / N, v = (float)y / N;
            float k = 0.86f + 0.22f * tex_h(x, y, 7);
            k *= 0.85f + 0.3f * fbm(u, v, 16, 2, 31);
            float hum = fbm(u, v, 3, 4, 47);
            if (hum > 0.55f) k *= 1 - 0.35f * clampf((hum - 0.55f) * 5, 0, 1);
            float sale = fbm(u, v, 8, 3, 59);
            float r = 0.53f, g = 0.46f, b = 0.25f;
            if (sale > 0.66f) { r *= 0.8f; g *= 0.74f; b *= 0.65f; }
            px_set(img, N, x, y, r * k, g * k, b * k);
        }
    upload_rgb_mip(&texMat[MAT_MOQUETTE], img, N);

    /* plafond (2,4 m = 4 x 4 dalles de 0,6 m) : rainures, pores, deux aureoles d'eau */
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float u = (float)x / N, v = (float)y / N;
            int tx = x % (N / 4), ty = y % (N / 4);
            float k = 1.0f;
            if (tx < 2 || ty < 2) k *= 0.6f;                               /* rainure entre dalles */
            else if (tx < 3 || ty < 3) k *= 1.06f;                         /* biseau eclaire */
            if (tex_h(x, y, 13) < 0.06f) k *= 0.82f;                       /* pores */
            k *= 0.95f + 0.08f * fbm(u, v, 8, 2, 17);
            float r = 0.80f, g = 0.78f, b = 0.68f;
            float cx = (x / (N / 4)) * 0.25f + 0.125f, cy = (y / (N / 4)) * 0.25f + 0.125f;
            int dalle = (x / (N / 4)) + 4 * (y / (N / 4));
            if (dalle == 5 || dalle == 14) {                               /* aureole brune, sur 2 dalles sur 16 */
                float d = sqrtf((u - cx) * (u - cx) + (v - cy) * (v - cy)) + 0.03f * fbm(u, v, 8, 2, 71);
                float bord = fabsf(d - 0.085f);
                if (d < 0.085f) { r *= 0.94f; g *= 0.91f; b *= 0.84f; }
                if (bord < 0.007f) { r *= 0.8f; g *= 0.74f; b *= 0.62f; }
            }
            px_set(img, N, x, y, r * k, g * k, b * k);
        }
    upload_rgb_mip(&texMat[MAT_PLAFOND], img, N);

    /* plinthe : vinyle brun, rayures de chaussures, poussiere */
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float u = (float)x / N, v = (float)y / N;
            float k = 0.85f + 0.15f * fbm(u, v, 8, 2, 101);
            if (tex_h(x / 2, y / 40, 103) < 0.03f) k *= 0.7f;   /* eraflures */
            if (fmodf(v * 8, 1.0f) < 0.06f) k *= 0.8f;    /* moulure */
            px_set(img, N, x, y, 0.33f * k, 0.25f * k, 0.16f * k);
        }
    upload_rgb_mip(&texMat[MAT_PLINTHE], img, N);

    /* corniche : platre blanc casse, jauni */
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float u = (float)x / N, v = (float)y / N;
            float k = 0.9f + 0.1f * fbm(u, v, 8, 2, 107);
            if (fmodf(v * 4, 1.0f) < 0.12f) k *= 0.82f;
            px_set(img, N, x, y, 0.84f * k, 0.8f * k, 0.66f * k);
        }
    upload_rgb_mip(&texMat[MAT_CORNICHE], img, N);

    /* cloison de bureau : tissu gris-bleu, trame, taches */
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float u = (float)x / N, v = (float)y / N;
            float k = 0.82f + 0.12f * tex_h(x, y, 109) + 0.08f * ((x + y) % 4 < 2);
            k *= 0.9f + 0.14f * fbm(u, v, 4, 3, 113);
            if (fbm(u, v, 3, 3, 127) > 0.64f) k *= 0.8f;
            float bord = fminf(u, 1 - u);
            if (bord < 0.02f) k *= 0.55f;                 /* montant metallique tous les 2 m */
            px_set(img, N, x, y, 0.36f * k, 0.38f * k, 0.42f * k);
        }
    upload_rgb_mip(&texMat[MAT_CLOISON], img, N);

    /* diffuseur de neon : blanc chaud, cadre metallique, petits prismes */
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float u = (float)x / N, v = (float)y / N;
            float bord = fminf(fminf(u, 1 - u), fminf(v, 1 - v));
            float k = 1.0f;
            if (bord < 0.06f) k = 0.45f + 0.1f * tex_h(x, y, 3);
            else {
                if ((x / 6 + y / 6) % 2) k *= 0.96f;
                k *= 1.0f - 0.25f * (1 - fminf(1, bord * 6));
            }
            px_set(img, N, x, y, 1.0f * k, 0.98f * k, 0.90f * k);
        }
    upload_rgb_mip(&texNeon, img, N);

    /* acier brosse (portes d'ascenseur) */
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float k = 0.8f + 0.1f * vnoise((float)x / N * 4, (float)y / N * 64, 64, 91) + 0.06f * tex_h(x, y, 93);
            if (tex_h(x / 3, y, 97) < 0.004f) k *= 1.3f;                   /* rayures */
            px_set(img, N, x, y, 0.52f * k, 0.53f * k, 0.55f * k);
        }
    upload_rgb_mip(&texMetal, img, N);
    free(img);

    /* faisceau de lampe (128) : coeur, halo, anneau du reflecteur, imperfections ; bord noir (CLAMP) */
    {
        const int L = 128;
        unsigned char *li = (unsigned char *)malloc((size_t)L * L * 3);
        for (int y = 0; y < L; y++)
            for (int x = 0; x < L; x++) {
                float dx = (x + 0.5f) / L - 0.5f, dy = (y + 0.5f) / L - 0.5f, r = sqrtf(dx * dx + dy * dy) * 2;   /* 0 centre, 1 bord */
                float k = 0;
                if (r < 1) {
                    float coeur = expf(-r * r * 9.0f);
                    float halo = 0.35f * (1 - r) * (1 - r);
                    float anneau = 0.18f * expf(-(r - 0.42f) * (r - 0.42f) * 300);
                    k = coeur + halo + anneau;
                    k *= 0.9f + 0.1f * vnoise(dx * 20 + 10, dy * 20 + 10, 64, 5);
                }
                if (x == 0 || y == 0 || x == L - 1 || y == L - 1) k = 0;
                px_set(li, L, x, y, k, k * 0.94f, k * 0.82f);
            }
        glGenTextures(1, &texLampe);
        glBindTexture(GL_TEXTURE_2D, texLampe);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F /* GL_CLAMP_TO_EDGE */);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, L, L, 0, GL_RGB, GL_UNSIGNED_BYTE, li);
        free(li);
    }
}
