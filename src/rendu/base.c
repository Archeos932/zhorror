/* BOURDONNEMENT - polices, camera et passage en 2D
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static GLuint fontBase[3];
static int fontW[3], fontH[3];

static int pow2up(int n) { int p = 1; while (p < n) p <<= 1; return p; }

/* camera a la premiere personne : projection perspective (sans GLU) puis vue */
static void set_camera(V3 eye, float yaw, float pitch) {
    float aspect = (float)winW / (float)(winH > 0 ? winH : 1);
    float top = CAM_NEAR * tanf(FOV_V * 0.5f * PI / 180.0f), right = top * aspect;
    glViewport(0, 0, winW, winH);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glFrustum(-right, right, -top, top, CAM_NEAR, CAM_FAR);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glRotatef(-pitch * 180.0f / PI, 1, 0, 0);
    glRotatef(yaw * 180.0f / PI, 0, 1, 0);
    glTranslatef(-eye.x, -eye.y, -eye.z);
}

/* 2D : pixels, origine en haut a gauche */
static void begin_2d(void) {
    glViewport(0, 0, winW, winH);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, winW, winH, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glDisable(GL_FOG); glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
