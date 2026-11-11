/* BOURDONNEMENT - son 3D : volume selon la distance, gauche / droite selon l'angle, etouffe derriere les murs
   Morceau de main.c : inclus dans l'ordre (apres monde/carte.c : il teste les murs), ne se compile pas seul. */

static V3 ecoutePos;                                      /* oreilles du joueur (mis a jour chaque tick) */
static float ecouteYaw;

/* gains gauche / droite et volume pour une source en p ; renvoie le volume total (0 = inaudible) */
static float son3d_gains(V3 p, float vol, float portee, float *gl, float *gr) {
    float dx = p.x - ecoutePos.x, dz = p.z - ecoutePos.z, d = sqrtf(dx * dx + dz * dz);
    float v = vol / (1.0f + d * d / (portee * portee) * 6.0f);
    if (d > 0.5f && !visible_xz(ecoutePos.x, ecoutePos.z, p.x, p.z)) v *= 0.45f;   /* derriere un mur : etouffe */
    float pan = 0;
    if (d > 0.05f) pan = (dx * cosf(ecouteYaw) + dz * sinf(ecouteYaw)) / d;          /* -1 gauche .. 1 droite */
    *gl = sqrtf(0.5f * (1 - pan * 0.85f)) * 1.41f;
    *gr = sqrtf(0.5f * (1 + pan * 0.85f)) * 1.41f;
    return v;
}

static void play3d(int s, V3 p, float vol, float pitch, float portee) {
    float gl, gr, v = son3d_gains(p, vol, portee, &gl, &gr);
    if (v < 0.004f) return;
    voice_pan(play_voice(s, v, pitch), gl, gr);
}

/* boucle placee dans le monde : a rappeler chaque image (v = valeur de play_loop) */
static void loop3d(int v, V3 p, float vol, float portee) {
    if (v <= 0) return;
    float gl, gr, g = son3d_gains(p, vol, portee, &gl, &gr);
    loop_set(v, g, 1.0f);
    voice_pan(v - 1, gl, gr);
}
