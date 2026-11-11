/* BOURDONNEMENT - polices, camera et passage en 2D
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static GLuint fontBase[3];
static int fontW[3], fontH[3];

static GLfloat mvMat[16], prMat[16];                      /* matrices de la camera (projeter un point a l'ecran) */
static float camYaw, camPitch, camRoll;
static V3 camEye;                                         /* oeil de l'image en cours (render) */

static int pow2up(int n) { int p = 1; while (p < n) p <<= 1; return p; }

/* camera a la premiere personne : projection perspective (sans GLU) puis vue */
static void set_camera(V3 eye, float yaw, float pitch, float roll, float fov) {
    float aspect = (float)winW / (float)(winH > 0 ? winH : 1);
    float top = CAM_NEAR * tanf(fov * 0.5f * PI / 180.0f), right = top * aspect;
    glViewport(0, 0, winW, winH);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glFrustum(-right, right, -top, top, CAM_NEAR, CAM_FAR);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glRotatef(roll, 0, 0, 1);
    glRotatef(-pitch * 180.0f / PI, 1, 0, 0);
    glRotatef(yaw * 180.0f / PI, 0, 1, 0);
    glTranslatef(-eye.x, -eye.y, -eye.z);
    glGetFloatv(GL_MODELVIEW_MATRIX, mvMat); glGetFloatv(GL_PROJECTION_MATRIX, prMat);
    camYaw = yaw; camPitch = pitch; camRoll = roll;
}

/* point du monde -> pixels (0 si derriere la camera) */
static int projeter(V3 p, float *sx, float *sy) {
    float v[4] = { p.x, p.y, p.z, 1 }, e[4], c[4];
    for (int i = 0; i < 4; i++) e[i] = mvMat[i] * v[0] + mvMat[4 + i] * v[1] + mvMat[8 + i] * v[2] + mvMat[12 + i] * v[3];
    for (int i = 0; i < 4; i++) c[i] = prMat[i] * e[0] + prMat[4 + i] * e[1] + prMat[8 + i] * e[2] + prMat[12 + i] * e[3];
    if (c[3] <= 0.01f) return 0;
    *sx = (c[0] / c[3] * 0.5f + 0.5f) * winW; *sy = (1 - (c[1] / c[3] * 0.5f + 0.5f)) * winH;
    return 1;
}

/* 2D : pixels, origine en haut a gauche */
static void begin_2d(void) {
    glViewport(0, 0, winW, winH);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, winW, winH, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glDisable(GL_FOG); glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
