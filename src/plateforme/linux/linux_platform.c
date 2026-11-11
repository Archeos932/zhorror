/*
 * BOURDONNEMENT (repris de VELOCITY) - couche plateforme Linux : fenetre X11 + GLX (pipeline fixe), clavier/souris, manette (/dev/input/js*),
 * son (ALSA), police bitmap (FreeType, repli police X) et WebSocket du lobby (sockets + OpenSSL charge a la volee).
 * Implemente la meme API que la couche macOS (mac_platform.h) : main.c n'a rien de propre a Linux.
 * Fonctionne aussi sous Wayland via XWayland. Compile par build_linux.sh.
 */
#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <alsa/asoundlib.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "../mac/mac_platform.h"

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* ------------------------------------------------------------------ */
/* Clavier : code X (evdev + 8) -> scancode Windows (set 1)            */
/* Les codes Linux 1..88 sont deja les scancodes set 1 ; les touches   */
/* etendues prennent le scancode sans prefixe, comme sous Windows.     */
/* ------------------------------------------------------------------ */
static int sc_of_keycode(unsigned kc) {
    int k = (int)kc - 8;
    if (k >= 1 && k <= 88) return k;
    switch (k) {
    case 96: return 0x1C;                                 /* Entree du pave numerique */
    case 97: return 0x1D;                                 /* Ctrl droit */
    case 98: return 0x35;                                 /* / du pave numerique */
    case 100: return 0x38;                                /* Alt Gr */
    case 102: return 0x47;                                /* Debut */
    case 103: return 0x48;                                /* fleches */
    case 104: return 0x49;                                /* Page haut */
    case 105: return 0x4B;
    case 106: return 0x4D;
    case 107: return 0x4F;                                /* Fin */
    case 108: return 0x50;
    case 109: return 0x51;                                /* Page bas */
    case 110: return 0x52;                                /* Inser */
    case 111: return 0x53;                                /* Suppr */
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Fenetre                                                             */
/* ------------------------------------------------------------------ */
static Display *gDpy;
static Window gWin;
static GLXContext gCtx;
static XVisualInfo *gVi;
static Colormap gCmap;
static Atom aDelete, aState, aFull;
static Cursor gBlank;
static int gQuit, gCaptured, gFull, gFocus, gVisible, gW, gH, gSoftGL, gLastX, gLastY;
static unsigned char gDown[256];                          /* touches enfoncees (repetition) */

void mac_gl_software(int on) { gSoftGL = on; }

static void set_swap_interval(Window w, int n) {
    typedef void (*SwapExt)(Display *, GLXDrawable, int);
    typedef int (*SwapMesa)(unsigned);
    SwapExt ext = (SwapExt)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalEXT");
    if (ext) { ext(gDpy, w, n); return; }
    SwapMesa mesa = (SwapMesa)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalMESA");
    if (mesa) mesa((unsigned)n);
}

int mac_window_create(int w, int h, const char *title, int visible) {
    if (gSoftGL) setenv("LIBGL_ALWAYS_SOFTWARE", "1", 1);  /* --soft : rendu logiciel de Mesa */
    gDpy = XOpenDisplay(NULL);
    if (!gDpy) { fprintf(stderr, "BOURDONNEMENT : pas d'affichage X11 (variable DISPLAY ; sous Wayland, XWayland est requis)\n"); return 0; }
    int scr = DefaultScreen(gDpy);
    /* anticrenelage 4x si la carte le permet (pas pour les bots caches) ; sinon, format simple */
    int ms[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, GLX_DEPTH_SIZE, 24,
                 GLX_STENCIL_SIZE, 8, GLX_SAMPLE_BUFFERS, 1, GLX_SAMPLES, 4, None };
    int at[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, GLX_DEPTH_SIZE, 24,
                 GLX_STENCIL_SIZE, 8, None };
    int lo[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 16, None };
    gVi = visible && !gSoftGL ? glXChooseVisual(gDpy, scr, ms) : NULL;
    if (!gVi) gVi = glXChooseVisual(gDpy, scr, at);
    if (!gVi) gVi = glXChooseVisual(gDpy, scr, lo);
    if (!gVi) { fprintf(stderr, "BOURDONNEMENT : aucun format OpenGL (GLX) disponible\n"); return 0; }
    Window root = RootWindow(gDpy, gVi->screen);
    int sw = DisplayWidth(gDpy, gVi->screen), sh = DisplayHeight(gDpy, gVi->screen);
    if (w > sw) w = sw;
    if (h > sh - 30) h = sh - 30;
    gCmap = XCreateColormap(gDpy, root, gVi->visual, AllocNone);
    XSetWindowAttributes swa;
    memset(&swa, 0, sizeof swa);
    swa.colormap = gCmap;
    swa.background_pixel = 0;
    swa.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                     StructureNotifyMask | FocusChangeMask | ExposureMask;
    gWin = XCreateWindow(gDpy, root, (sw - w) / 2, (sh - h) / 2, (unsigned)w, (unsigned)h, 0, gVi->depth, InputOutput,
                         gVi->visual, CWColormap | CWEventMask | CWBackPixel, &swa);
    if (!gWin) return 0;
    XStoreName(gDpy, gWin, title);
    XClassHint ch = { (char *)"bourdonnement", (char *)"Bourdonnement" };
    XSetClassHint(gDpy, gWin, &ch);
    aDelete = XInternAtom(gDpy, "WM_DELETE_WINDOW", False);
    aState = XInternAtom(gDpy, "_NET_WM_STATE", False);
    aFull = XInternAtom(gDpy, "_NET_WM_STATE_FULLSCREEN", False);
    XSetWMProtocols(gDpy, gWin, &aDelete, 1);
    Bool det;
    XkbSetDetectableAutoRepeat(gDpy, True, &det);         /* repetition : appuis seuls, sans faux relachements */
    {                                                     /* curseur invisible (souris captee) */
        static char zero[8];
        XColor black;
        memset(&black, 0, sizeof black);
        Pixmap pm = XCreateBitmapFromData(gDpy, gWin, zero, 8, 8);
        gBlank = XCreatePixmapCursor(gDpy, pm, pm, &black, &black, 0, 0);
        XFreePixmap(gDpy, pm);
    }
    gCtx = glXCreateContext(gDpy, gVi, NULL, True);
    if (!gCtx) { fprintf(stderr, "BOURDONNEMENT : impossible de creer le contexte OpenGL\n"); return 0; }
    gW = w; gH = h;
    gVisible = visible;
    if (visible) {
        XMapRaised(gDpy, gWin);
        for (int tries = 0; tries < 200; tries++) {       /* attendre l'affichage (au plus ~2 s) */
            XEvent e;
            if (XCheckTypedWindowEvent(gDpy, gWin, MapNotify, &e)) break;
            usleep(10000);
        }
    }
    glXMakeCurrent(gDpy, gWin, gCtx);
    set_swap_interval(gWin, 1);
    XFlush(gDpy);
    return 1;
}

void mac_window_place(int x, int y, int cw, int ch) {
    XMoveResizeWindow(gDpy, gWin, x, y, (unsigned)cw, (unsigned)ch);
    gW = cw; gH = ch;
    XFlush(gDpy);
}

void mac_window_size(int *w, int *h) {
    Window r;
    int x, y;
    unsigned ww, hh, b, d;
    if (XGetGeometry(gDpy, gWin, &r, &x, &y, &ww, &hh, &b, &d)) { gW = (int)ww; gH = (int)hh; }
    *w = gW; *h = gH;
}

void mac_swap(void) { glXSwapBuffers(gDpy, gWin); }

int mac_has_focus(void) { return gFocus; }

void mac_quit(void) { gQuit = 1; }

void mac_set_fullscreen(int on) {                         /* plein ecran du gestionnaire de fenetres (_NET_WM_STATE) */
    if (on == gFull) return;
    XEvent e;
    memset(&e, 0, sizeof e);
    e.xclient.type = ClientMessage;
    e.xclient.window = gWin;
    e.xclient.message_type = aState;
    e.xclient.format = 32;
    e.xclient.data.l[0] = on ? 1 : 0;                     /* _NET_WM_STATE_ADD / REMOVE */
    e.xclient.data.l[1] = (long)aFull;
    e.xclient.data.l[3] = 1;
    XSendEvent(gDpy, DefaultRootWindow(gDpy), False, SubstructureRedirectMask | SubstructureNotifyMask, &e);
    XFlush(gDpy);
    gFull = on;
}

static void warp_center(void) {
    XWarpPointer(gDpy, None, gWin, 0, 0, 0, 0, gW / 2, gH / 2);
    gLastX = gW / 2; gLastY = gH / 2;
    XFlush(gDpy);
}

void mac_set_capture(int on) {                            /* souris captee : curseur cache, ramene au centre */
    if (on == gCaptured || !gVisible) return;
    gCaptured = on;
    if (on) {
        XGrabPointer(gDpy, gWin, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync,
                     gWin, gBlank, CurrentTime);
        XDefineCursor(gDpy, gWin, gBlank);
        warp_center();
    } else {
        XUngrabPointer(gDpy, CurrentTime);
        XUndefineCursor(gDpy, gWin);
        XFlush(gDpy);
    }
}

/* second ecran (carte sur un autre moniteur) : pas encore sous Linux */
int mac_map_update(int want) { (void)want; return 0; }
int mac_map_begin(int *w, int *h) { (void)w; (void)h; return 0; }
void mac_map_end(void) {}

/* ------------------------------------------------------------------ */
/* Manette : interface joystick du noyau (/dev/input/js0..3)           */
/* ------------------------------------------------------------------ */
static int gJs = -1, gJsSony;
static double gJsRetry;
static short gJsAx[16];
static unsigned char gJsBt[32];
static char gJsName[128];

static void js_open(void) {
    double t = now_s();
    if (t < gJsRetry) return;
    gJsRetry = t + 2;                                     /* une tentative toutes les 2 s tant qu'aucune n'est branchee */
    for (int i = 0; i < 4 && gJs < 0; i++) {
        char p[32];
        snprintf(p, sizeof p, "/dev/input/js%d", i);
        gJs = open(p, O_RDONLY | O_NONBLOCK);
    }
    if (gJs < 0) return;
    memset(gJsAx, 0, sizeof gJsAx);
    memset(gJsBt, 0, sizeof gJsBt);
    if (ioctl(gJs, JSIOCGNAME(sizeof gJsName), gJsName) < 0) snprintf(gJsName, sizeof gJsName, "manette");
    gJsSony = strstr(gJsName, "Sony") || strstr(gJsName, "DualSense") || strstr(gJsName, "Wireless Controller") ||
              strstr(gJsName, "PS4") || strstr(gJsName, "PS5");
}

int mac_pad_read(float ax[6], unsigned *btn, char *name, int n) {
    if (gJs < 0) js_open();
    if (gJs < 0) return 0;
    struct js_event e;
    for (;;) {
        ssize_t r = read(gJs, &e, sizeof e);
        if (r != (ssize_t)sizeof e) {
            if (r < 0 && errno != EAGAIN) { close(gJs); gJs = -1; return 0; }   /* debranchee */
            break;
        }
        int ty = e.type & ~JS_EVENT_INIT;
        if (ty == JS_EVENT_AXIS && e.number < 16) gJsAx[e.number] = e.value;
        else if (ty == JS_EVENT_BUTTON && e.number < 32) gJsBt[e.number] = (unsigned char)(e.value != 0);
    }
    /* disposition du pilote : xpad (Xbox) ou hid-sony / hid-playstation ; axes 0 1 stick G, 3 4 stick D, 2 5 gachettes, 6 7 croix */
    ax[0] = gJsAx[0] / 32767.0f; ax[1] = -gJsAx[1] / 32767.0f;
    ax[2] = gJsAx[3] / 32767.0f; ax[3] = -gJsAx[4] / 32767.0f;
    ax[4] = (gJsAx[2] + 32767) / 65534.0f; ax[5] = (gJsAx[5] + 32767) / 65534.0f;
    unsigned b = 0;
#define PB(i, cond) if (cond) b |= 1u << (i)
    if (gJsSony) {                                        /* croix rond triangle carre L1 R1 L2 R2 share options PS L3 R3 */
        PB(0, gJsBt[0]); PB(1, gJsBt[1]); PB(3, gJsBt[2]); PB(2, gJsBt[3]); PB(4, gJsBt[4]); PB(5, gJsBt[5]);
        PB(6, gJsBt[6]); PB(7, gJsBt[7]); PB(8, gJsBt[8]); PB(9, gJsBt[9]); PB(12, gJsBt[10]); PB(10, gJsBt[11]);
        PB(11, gJsBt[12]);
    } else {                                              /* A B X Y LB RB Back Start Guide LS RS */
        PB(0, gJsBt[0]); PB(1, gJsBt[1]); PB(2, gJsBt[2]); PB(3, gJsBt[3]); PB(4, gJsBt[4]); PB(5, gJsBt[5]);
        PB(8, gJsBt[6]); PB(9, gJsBt[7]); PB(12, gJsBt[8]); PB(10, gJsBt[9]); PB(11, gJsBt[10]);
    }
    PB(6, ax[4] > 0.5f); PB(7, ax[5] > 0.5f);
    PB(14, gJsAx[7] < -16000); PB(15, gJsAx[7] > 16000); PB(16, gJsAx[6] < -16000); PB(17, gJsAx[6] > 16000);
#undef PB
    *btn = b;
    if (name && n > 0) snprintf(name, (size_t)n, "%s", gJsName);
    return 1;
}

void mac_shutdown(void) {
    mac_set_capture(0);
    if (gFull) mac_set_fullscreen(0);
    if (gJs >= 0) { close(gJs); gJs = -1; }
    if (gDpy) {
        glXMakeCurrent(gDpy, None, NULL);
        if (gCtx) glXDestroyContext(gDpy, gCtx);
        if (gWin) XDestroyWindow(gDpy, gWin);
        XCloseDisplay(gDpy);
        gDpy = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Evenements                                                          */
/* ------------------------------------------------------------------ */
static void handle_event(XEvent *e) {
    switch (e->type) {
    case KeyPress: {
        int sc = sc_of_keycode(e->xkey.keycode);
        if (sc) {
            int rep = gDown[e->xkey.keycode & 255];
            gDown[e->xkey.keycode & 255] = 1;
            plat_key(sc, 1, rep);
        }
        char buf[16];
        KeySym ks;
        int n = XLookupString(&e->xkey, buf, sizeof buf, &ks, NULL);
        if (ks == XK_BackSpace) plat_char(8);
        else if (ks == XK_Return || ks == XK_KP_Enter) plat_char(13);
        else
            for (int i = 0; i < n; i++) {
                unsigned char c = (unsigned char)buf[i];
                if (c >= 32 && c < 127) plat_char(c);
            }
        return;
    }
    case KeyRelease: {
        int sc = sc_of_keycode(e->xkey.keycode);
        gDown[e->xkey.keycode & 255] = 0;
        if (sc) plat_key(sc, 0, 0);
        return;
    }
    case ButtonPress: case ButtonRelease: {
        int down = e->type == ButtonPress;
        unsigned b = e->xbutton.button;
        if (!gCaptured) plat_mouse_move(e->xbutton.x, e->xbutton.y);
        if (b == 4 || b == 5) { if (down) plat_wheel(); return; }   /* molette : un cran par evenement */
        if (b == 6 || b == 7) return;                     /* molette horizontale */
        int id = b == 1 ? 0 : b == 3 ? 1 : b == 2 ? 2 : b == 8 ? 3 : b == 9 ? 4 : -1;
        if (id >= 0) plat_button(id, down);
        return;
    }
    case MotionNotify: {
        int x = e->xmotion.x, y = e->xmotion.y;
        if (gCaptured) {
            int dx = x - gLastX, dy = y - gLastY;
            if (dx || dy) plat_raw_motion((float)dx, (float)dy);
            gLastX = x; gLastY = y;
        } else {
            plat_raw_motion((float)(x - gLastX), (float)(y - gLastY));
            gLastX = x; gLastY = y;
            plat_mouse_move(x, y);
        }
        return;
    }
    case ConfigureNotify:
        if (e->xconfigure.width != gW || e->xconfigure.height != gH) {
            gW = e->xconfigure.width; gH = e->xconfigure.height;
            plat_resize(gW, gH);
        }
        return;
    case FocusIn:
        gFocus = 1;
        return;
    case FocusOut:
        if (e->xfocus.mode == NotifyGrab || e->xfocus.mode == NotifyUngrab) return;   /* notre propre capture */
        gFocus = 0;
        memset(gDown, 0, sizeof gDown);
        plat_focus_lost();
        return;
    case ClientMessage:
        if ((Atom)e->xclient.data.l[0] == aDelete) gQuit = 1;
        return;
    }
}

int mac_pump(void) {
    while (XPending(gDpy)) {
        XEvent e;
        XNextEvent(gDpy, &e);
        handle_event(&e);
    }
    if (gCaptured && (abs(gLastX - gW / 2) > gW / 4 || abs(gLastY - gH / 2) > gH / 4)) warp_center();   /* loin du centre : on y revient */
    return !gQuit;
}

/* ------------------------------------------------------------------ */
/* Noms des touches                                                    */
/* ------------------------------------------------------------------ */
void mac_key_name(int sc, char *out, int n) {
    static const struct { int sc; const char *name; } special[] = {
        { 0x01, "Echap" }, { 0x0E, "Retour" }, { 0x0F, "Tab" }, { 0x1C, "Entree" }, { 0x1D, "Ctrl" }, { 0x2A, "Maj" },
        { 0x36, "Maj droite" }, { 0x38, "Alt" }, { 0x39, "Espace" }, { 0x3A, "Verr Maj" },
        { 0x3B, "F1" }, { 0x3C, "F2" }, { 0x3D, "F3" }, { 0x3E, "F4" }, { 0x3F, "F5" }, { 0x40, "F6" }, { 0x41, "F7" },
        { 0x42, "F8" }, { 0x43, "F9" }, { 0x44, "F10" }, { 0x57, "F11" }, { 0x58, "F12" },
        { 0x48, "Fleche haut" }, { 0x50, "Fleche bas" }, { 0x4B, "Fleche gauche" }, { 0x4D, "Fleche droite" },
        { 0x47, "Debut" }, { 0x4F, "Fin" }, { 0x49, "Page haut" }, { 0x51, "Page bas" }, { 0x53, "Suppr" },
    };
    for (size_t i = 0; i < sizeof special / sizeof special[0]; i++)
        if (special[i].sc == sc) { snprintf(out, (size_t)n, "%s", special[i].name); return; }
    if (gDpy && sc >= 1 && sc <= 88) {                    /* caractere de la touche selon la disposition courante */
        KeySym ks = XkbKeycodeToKeysym(gDpy, (KeyCode)(sc + 8), 0, 0);
        if (ks > 32 && ks < 256) {                        /* Latin-1 : meme valeur qu'en CP1252 */
            snprintf(out, (size_t)n, "%c", ks < 127 ? toupper((int)ks) : (int)ks);
            return;
        }
    }
    snprintf(out, (size_t)n, "touche %d", sc);
}

/* ------------------------------------------------------------------ */
/* Police bitmap : 224 listes d'affichage glBitmap, comme wglUseFontBitmaps */
/* ------------------------------------------------------------------ */
static const unsigned short cp1252[32] = {              /* 0x80..0x9F -> Unicode */
    0x20AC, 0x3F, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x3F, 0x017D, 0x3F,
    0x3F, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x3F, 0x017E, 0x0178,
};

static int font_path(char *out, int n) {                  /* une police a chasse fixe et grasse, presente partout ou presque */
    static const char *cands[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf",
        "/usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono-Bold.ttf",
        "/usr/share/fonts/dejavu/DejaVuSansMono-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Bold.ttf",
        "/usr/share/fonts/liberation-mono/LiberationMono-Bold.ttf",
        "/usr/share/fonts/truetype/noto/NotoSansMono-Bold.ttf",
        "/usr/share/fonts/noto/NotoSansMono-Bold.ttf",
    };
    for (size_t i = 0; i < sizeof cands / sizeof cands[0]; i++)
        if (access(cands[i], R_OK) == 0) { snprintf(out, (size_t)n, "%s", cands[i]); return 1; }
    FILE *p = popen("fc-match -f '%{file}' 'monospace:bold' 2>/dev/null", "r");   /* fontconfig */
    if (!p) return 0;
    int len = (int)fread(out, 1, (size_t)n - 1, p);
    pclose(p);
    if (len <= 0) return 0;
    out[len] = 0;
    return access(out, R_OK) == 0;
}

static int x_font(unsigned base, int px, int *cw, int *ch) {   /* repli : police X du serveur */
    char nm[128];
    snprintf(nm, sizeof nm, "-*-*-bold-r-*-*-%d-*-*-*-m-*-iso8859-1", px);
    XFontStruct *f = XLoadQueryFont(gDpy, nm);
    if (!f) f = XLoadQueryFont(gDpy, "-*-fixed-bold-r-*-*-*-*-*-*-*-*-iso8859-1");
    if (!f) f = XLoadQueryFont(gDpy, "fixed");
    if (!f) return 0;
    glXUseXFont(f->fid, 32, 224, (int)base);
    *cw = f->max_bounds.width; *ch = f->ascent + f->descent;
    XFreeFont(gDpy, f);
    return 1;
}

int mac_make_font(unsigned base, int px, int *cw, int *ch) {
    static FT_Library lib;
    static FT_Face face;
    static int tried;
    if (!tried) {
        char path[512];
        tried = 1;
        if (FT_Init_FreeType(&lib) || !font_path(path, sizeof path) || FT_New_Face(lib, path, 0, &face)) face = NULL;
    }
    if (!face || FT_Set_Pixel_Sizes(face, 0, (FT_UInt)(px * 0.92f + 0.5f))) return x_font(base, px, cw, ch);
    int W = (int)(px * 0.55f);
    if (!FT_Load_Char(face, 'M', FT_LOAD_DEFAULT)) W = (int)((face->glyph->advance.x + 63) >> 6);
    int A = (int)((face->size->metrics.ascender + 63) >> 6), D = (int)((-face->size->metrics.descender + 63) >> 6);
    int H = A + D, bpr = (W + 7) / 8;
    unsigned char *bits = (unsigned char *)malloc((size_t)bpr * H);
    GLint oldAlign;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &oldAlign);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (int c = 32; c < 256; c++) {
        memset(bits, 0, (size_t)bpr * H);
        unsigned uc = c >= 0x80 && c < 0xA0 ? cp1252[c - 0x80] : (unsigned)c;
        if (c != 32 && !FT_Load_Char(face, uc, FT_LOAD_RENDER)) {
            FT_Bitmap *bm = &face->glyph->bitmap;
            int x0 = face->glyph->bitmap_left, y0 = A - face->glyph->bitmap_top;   /* coin haut gauche, depuis le haut */
            for (int r = 0; r < (int)bm->rows; r++)
                for (int x = 0; x < (int)bm->width; x++) {
                    int px2 = x0 + x, py = y0 + r;
                    if (px2 < 0 || px2 >= W || py < 0 || py >= H) continue;
                    unsigned char v = bm->pixel_mode == FT_PIXEL_MODE_MONO
                                          ? (unsigned char)((bm->buffer[r * bm->pitch + x / 8] >> (7 - (x & 7))) & 1 ? 255 : 0)
                                          : bm->buffer[r * bm->pitch + x];
                    if (v >= 100) bits[(size_t)(H - 1 - py) * bpr + px2 / 8] |= (unsigned char)(0x80 >> (px2 & 7));   /* glBitmap : bas d'abord */
                }
        }
        glNewList(base + (unsigned)(c - 32), GL_COMPILE);
        glBitmap(W, H, 0, (GLfloat)D, (GLfloat)W, 0, bits);
        glEndList();
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, oldAlign);
    free(bits);
    *cw = W; *ch = H;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Audio : ALSA ; un fil envoie les tampons remplis par le fil principal */
/* ------------------------------------------------------------------ */
#define MAX_AUDIO_BUF 8
static snd_pcm_t *gPcm;
static pthread_t gAudioTh;
static pthread_mutex_t gAm = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gAc = PTHREAD_COND_INITIALIZER;
static short *gAbuf[MAX_AUDIO_BUF];
static int gFilled[MAX_AUDIO_BUF], gNbuf, gFrames, gAudioStop;
static atomic_int gDone[MAX_AUDIO_BUF];

static void *audio_main(void *arg) {
    (void)arg;
    int i = 0;
    for (;;) {
        pthread_mutex_lock(&gAm);
        while (!gFilled[i] && !gAudioStop) pthread_cond_wait(&gAc, &gAm);
        int stop = gAudioStop;
        pthread_mutex_unlock(&gAm);
        if (stop) break;
        const short *p = gAbuf[i];
        int left = gFrames;
        while (left > 0) {
            snd_pcm_sframes_t w = snd_pcm_writei(gPcm, p, (snd_pcm_uframes_t)left);
            if (w < 0) {
                if (snd_pcm_recover(gPcm, (int)w, 1) < 0) { usleep(10000); break; }   /* sous-alimentation, veille... */
                continue;
            }
            p += w * 2; left -= (int)w;
        }
        pthread_mutex_lock(&gAm);
        gFilled[i] = 0;
        pthread_mutex_unlock(&gAm);
        atomic_store(&gDone[i], 1);
        i = (i + 1) % gNbuf;
    }
    return NULL;
}

int mac_audio_open(int rate, int nbuf, int buflen) {
    if (nbuf > MAX_AUDIO_BUF) return 0;
    if (snd_pcm_open(&gPcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) { gPcm = NULL; return 0; }
    if (snd_pcm_set_params(gPcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 2, (unsigned)rate, 1, 30000) < 0) {
        snd_pcm_close(gPcm); gPcm = NULL; return 0;
    }
    gNbuf = nbuf; gFrames = buflen; gAudioStop = 0;
    for (int i = 0; i < nbuf; i++) {
        gAbuf[i] = (short *)calloc((size_t)buflen * 2, sizeof(short));
        gFilled[i] = 0;
        atomic_store(&gDone[i], 0);
    }
    if (pthread_create(&gAudioTh, NULL, audio_main, NULL) != 0) { snd_pcm_close(gPcm); gPcm = NULL; return 0; }
    return 1;
}

int mac_audio_done(int i) { return atomic_load(&gDone[i]); }

void mac_audio_write(int i, const short *samples) {
    atomic_store(&gDone[i], 0);
    memcpy(gAbuf[i], samples, (size_t)gFrames * 4);
    pthread_mutex_lock(&gAm);
    gFilled[i] = 1;
    pthread_cond_signal(&gAc);
    pthread_mutex_unlock(&gAm);
}

void mac_audio_close(void) {
    if (!gPcm) return;
    pthread_mutex_lock(&gAm);
    gAudioStop = 1;
    pthread_cond_signal(&gAc);
    pthread_mutex_unlock(&gAm);
    pthread_join(gAudioTh, NULL);
    snd_pcm_drop(gPcm);
    snd_pcm_close(gPcm);
    gPcm = NULL;
    for (int i = 0; i < gNbuf; i++) { free(gAbuf[i]); gAbuf[i] = NULL; }
}

/* ------------------------------------------------------------------ */
/* WebSocket du lobby : TCP + TLS (libssl chargee a la volee) + trames */
/* Lecture (fil lb_reader) et ecriture (fil lb_writer) en parallele : */
/* socket non bloquante, un verrou autour de chaque appel TLS.        */
/* ------------------------------------------------------------------ */
typedef struct {
    void *(*TLS_client_method)(void);
    void *(*SSL_CTX_new)(const void *);
    void (*SSL_CTX_free)(void *);
    int (*SSL_CTX_set_default_verify_paths)(void *);
    void (*SSL_CTX_set_verify)(void *, int, void *);
    void *(*SSL_new)(void *);
    void (*SSL_free)(void *);
    int (*SSL_set_fd)(void *, int);
    long (*SSL_ctrl)(void *, int, long, void *);
    int (*SSL_set1_host)(void *, const char *);
    int (*SSL_connect)(void *);
    int (*SSL_read)(void *, void *, int);
    int (*SSL_write)(void *, const void *, int);
    int (*SSL_get_error)(const void *, int);
    int (*SSL_shutdown)(void *);
} SslApi;
static SslApi ssl;
static int sslState;                                      /* 0 pas essaye, 1 pret, -1 absente */
static pthread_mutex_t sslInit = PTHREAD_MUTEX_INITIALIZER;

static int ssl_load(void) {
    pthread_mutex_lock(&sslInit);
    if (!sslState) {
        static const char *libs[] = { "libssl.so.3", "libssl.so.1.1", "libssl.so" };
        void *h = NULL;
        for (size_t i = 0; i < sizeof libs / sizeof libs[0] && !h; i++) h = dlopen(libs[i], RTLD_NOW | RTLD_LOCAL);
        sslState = -1;
        if (h) {
#define L(f) *(void **)&ssl.f = dlsym(h, #f)
            L(TLS_client_method); L(SSL_CTX_new); L(SSL_CTX_free); L(SSL_CTX_set_default_verify_paths); L(SSL_CTX_set_verify);
            L(SSL_new); L(SSL_free); L(SSL_set_fd); L(SSL_ctrl); L(SSL_set1_host); L(SSL_connect); L(SSL_read);
            L(SSL_write); L(SSL_get_error); L(SSL_shutdown);
#undef L
            if (ssl.TLS_client_method && ssl.SSL_CTX_new && ssl.SSL_new && ssl.SSL_set_fd && ssl.SSL_ctrl && ssl.SSL_connect &&
                ssl.SSL_read && ssl.SSL_write && ssl.SSL_get_error && ssl.SSL_free && ssl.SSL_CTX_free)
                sslState = 1;
        }
        if (sslState < 0) fprintf(stderr, "BOURDONNEMENT : libssl introuvable, lobby en ligne indisponible (installer OpenSSL)\n");
    }
    pthread_mutex_unlock(&sslInit);
    return sslState > 0;
}

#define SSL_ERR_WANT_READ 2
#define SSL_ERR_WANT_WRITE 3

struct MacWs {
    int fd;
    void *ctx, *s;                                        /* SSL_CTX, SSL (NULL en ws:// clair) */
    pthread_mutex_t m;
    atomic_int closed;
};

/* une lecture ou une ecriture ; >0 octets, 0 = reessayer (want : POLLIN / POLLOUT), -1 = fermee */
static int ws_io(MacWs *w, void *buf, int n, int wr, short *want) {
    int r;
    pthread_mutex_lock(&w->m);
    if (w->s) {
        r = wr ? ssl.SSL_write(w->s, buf, n) : ssl.SSL_read(w->s, buf, n);
        if (r <= 0) {
            int e = ssl.SSL_get_error(w->s, r);
            if (e == SSL_ERR_WANT_READ) { *want = POLLIN; r = 0; }
            else if (e == SSL_ERR_WANT_WRITE) { *want = POLLOUT; r = 0; }
            else r = -1;
        }
    } else {
        r = (int)(wr ? send(w->fd, buf, (size_t)n, MSG_NOSIGNAL) : recv(w->fd, buf, (size_t)n, 0));
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) { *want = wr ? POLLOUT : POLLIN; r = 0; }
        else if (r == 0 && !wr) r = -1;                   /* fermee par le serveur */
    }
    pthread_mutex_unlock(&w->m);
    return r;
}

static int ws_write_all(MacWs *w, const void *buf, int n) {
    const char *p = (const char *)buf;
    double t0 = now_s();
    while (n > 0) {
        short want = POLLOUT;
        if (atomic_load(&w->closed)) return -1;
        int r = ws_io(w, (void *)p, n, 1, &want);
        if (r < 0) return -1;
        if (r > 0) { p += r; n -= r; continue; }
        if (now_s() - t0 > 10) return -1;
        struct pollfd pf = { w->fd, want, 0 };
        poll(&pf, 1, 100);
    }
    return 0;
}

static int ws_read_full(MacWs *w, void *buf, int n, double timeout) {   /* n octets exactement ; -1 = fermee ou delai */
    char *p = (char *)buf;
    double t0 = now_s();
    while (n > 0) {
        short want = POLLIN;
        if (atomic_load(&w->closed)) return -1;
        int r = ws_io(w, p, n, 0, &want);
        if (r < 0) return -1;
        if (r > 0) { p += r; n -= r; t0 = now_s(); continue; }
        if (now_s() - t0 > timeout) return -1;
        struct pollfd pf = { w->fd, want, 0 };
        poll(&pf, 1, 200);
    }
    return 0;
}

static int ws_frame(MacWs *w, int op, const void *data, int n) {   /* trame client (masquee) */
    unsigned char hdr[14], mask[4];
    int h = 0;
    hdr[h++] = (unsigned char)(0x80 | op);
    if (n < 126) hdr[h++] = (unsigned char)(0x80 | n);
    else if (n < 65536) { hdr[h++] = 0x80 | 126; hdr[h++] = (unsigned char)(n >> 8); hdr[h++] = (unsigned char)n; }
    else {
        hdr[h++] = 0x80 | 127;
        for (int i = 7; i >= 0; i--) hdr[h++] = (unsigned char)(i < 4 ? ((unsigned)n >> (i * 8)) : 0);
    }
    for (int i = 0; i < 4; i++) mask[i] = (unsigned char)(rand() & 255);
    memcpy(hdr + h, mask, 4); h += 4;
    unsigned char *buf = (unsigned char *)malloc((size_t)h + (size_t)n);
    if (!buf) return -1;
    memcpy(buf, hdr, (size_t)h);
    for (int i = 0; i < n; i++) buf[h + i] = ((const unsigned char *)data)[i] ^ mask[i & 3];
    pthread_mutex_lock(&w->m);                            /* une trame entiere a la fois (pong du lecteur / envoi de l'ecrivain) */
    int r = ws_write_all(w, buf, h + n);
    pthread_mutex_unlock(&w->m);
    free(buf);
    return r;
}

static void b64(const unsigned char *in, int n, char *out) {
    static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int o = 0;
    for (int i = 0; i < n; i += 3) {
        unsigned v = (unsigned)in[i] << 16 | (i + 1 < n ? (unsigned)in[i + 1] << 8 : 0) | (i + 2 < n ? in[i + 2] : 0);
        out[o++] = t[v >> 18 & 63]; out[o++] = t[v >> 12 & 63];
        out[o++] = i + 1 < n ? t[v >> 6 & 63] : '=';
        out[o++] = i + 2 < n ? t[v & 63] : '=';
    }
    out[o] = 0;
}

static void ws_destroy(MacWs *w) {
    if (w->s) ssl.SSL_free(w->s);
    if (w->ctx) ssl.SSL_CTX_free(w->ctx);
    if (w->fd >= 0) close(w->fd);
    pthread_mutex_destroy(&w->m);
    free(w);
}

MacWs *mac_ws_open(const char *host, int port, int secure) {
    if (secure && !ssl_load()) return NULL;
    struct addrinfo hints, *res = NULL, *a;
    char ps[16];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    snprintf(ps, sizeof ps, "%d", port);
    if (getaddrinfo(host, ps, &hints, &res) != 0) return NULL;
    MacWs *w = (MacWs *)calloc(1, sizeof *w);
    pthread_mutexattr_t ma;                               /* recursif : ws_frame garde la trame entiere, ws_io reprend le verrou */
    pthread_mutexattr_init(&ma);
    pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&w->m, &ma);
    pthread_mutexattr_destroy(&ma);
    w->fd = -1;
    for (a = res; a && w->fd < 0; a = a->ai_next) {       /* connexion avec delai de 8 s */
        int fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0) continue;
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
        if (connect(fd, a->ai_addr, a->ai_addrlen) == 0) { w->fd = fd; break; }
        if (errno == EINPROGRESS) {
            struct pollfd pf = { fd, POLLOUT, 0 };
            int err = 0;
            socklen_t el = sizeof err;
            if (poll(&pf, 1, 8000) == 1 && getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &el) == 0 && !err) { w->fd = fd; break; }
        }
        close(fd);
    }
    freeaddrinfo(res);
    if (w->fd < 0) { ws_destroy(w); return NULL; }
    int one = 1;
    setsockopt(w->fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    if (secure) {
        w->ctx = ssl.SSL_CTX_new(ssl.TLS_client_method());
        if (!w->ctx) { ws_destroy(w); return NULL; }
        if (ssl.SSL_CTX_set_default_verify_paths) ssl.SSL_CTX_set_default_verify_paths(w->ctx);
        if (ssl.SSL_CTX_set_verify) ssl.SSL_CTX_set_verify(w->ctx, 1 /* SSL_VERIFY_PEER */, NULL);
        w->s = ssl.SSL_new(w->ctx);
        if (!w->s) { ws_destroy(w); return NULL; }
        ssl.SSL_set_fd(w->s, w->fd);
        ssl.SSL_ctrl(w->s, 55 /* SSL_CTRL_SET_TLSEXT_HOSTNAME */, 0 /* nom d'hote */, (void *)host);
        if (ssl.SSL_set1_host) ssl.SSL_set1_host(w->s, host);   /* le certificat doit etre celui de l'hote */
        double t0 = now_s();
        for (;;) {
            int r = ssl.SSL_connect(w->s);
            if (r == 1) break;
            int e = ssl.SSL_get_error(w->s, r);
            if ((e != SSL_ERR_WANT_READ && e != SSL_ERR_WANT_WRITE) || now_s() - t0 > 8) { ws_destroy(w); return NULL; }
            struct pollfd pf = { w->fd, (short)(e == SSL_ERR_WANT_READ ? POLLIN : POLLOUT), 0 };
            poll(&pf, 1, 200);
        }
    }
    /* poignee de main HTTP -> WebSocket */
    unsigned char key[16];
    char k64[32], req[512];
    FILE *ur = fopen("/dev/urandom", "rb");
    if (!ur || fread(key, 1, sizeof key, ur) != sizeof key)
        for (int i = 0; i < 16; i++) key[i] = (unsigned char)(rand() ^ (int)(now_s() * 1e6));
    if (ur) fclose(ur);
    srand((unsigned)key[0] << 8 ^ key[1] ^ (unsigned)getpid());
    b64(key, 16, k64);
    int rn = snprintf(req, sizeof req,
                      "GET / HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                      "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\nUser-Agent: BOURDONNEMENT\r\n\r\n", host, k64);
    if (ws_write_all(w, req, rn) != 0) { ws_destroy(w); return NULL; }
    char resp[2048];
    int got = 0;
    while (got < (int)sizeof resp - 1) {                  /* en-tetes octet par octet jusqu'a la ligne vide */
        if (ws_read_full(w, resp + got, 1, 8) != 0) { ws_destroy(w); return NULL; }
        got++;
        resp[got] = 0;
        if (got >= 4 && !memcmp(resp + got - 4, "\r\n\r\n", 4)) break;
    }
    if (strncmp(resp, "HTTP/1.1 101", 12) != 0) { ws_destroy(w); return NULL; }
    return w;
}

int mac_ws_send(MacWs *w, const char *s, int n) {
    if (!w || atomic_load(&w->closed)) return -1;
    return ws_frame(w, 1, s, n) == 0 ? 0 : -1;
}

int mac_ws_recv(MacWs *w, char *buf, int cap) {
    int len = 0, tooLong = 0;
    for (;;) {
        unsigned char h[2], ext[8];
        /* le serveur repond a nos "ping" : 60 s sans rien = connexion morte */
        if (ws_read_full(w, h, 2, 60) != 0) return -1;
        int fin = h[0] & 0x80, op = h[0] & 15, masked = h[1] & 0x80;
        uint64_t n = h[1] & 127;
        if (n == 126) { if (ws_read_full(w, ext, 2, 10)) return -1; n = (uint64_t)ext[0] << 8 | ext[1]; }
        else if (n == 127) {
            if (ws_read_full(w, ext, 8, 10)) return -1;
            n = 0;
            for (int i = 0; i < 8; i++) n = n << 8 | ext[i];
        }
        unsigned char mk[4] = { 0, 0, 0, 0 };
        if (masked && ws_read_full(w, mk, 4, 10)) return -1;
        if (n > (1u << 24)) return -1;                    /* 16 Mo : absurde pour le lobby */
        unsigned char *pl = (unsigned char *)malloc((size_t)n + 1);
        if (!pl) return -1;
        if (n && ws_read_full(w, pl, (int)n, 10)) { free(pl); return -1; }
        if (masked) for (uint64_t i = 0; i < n; i++) pl[i] ^= mk[i & 3];
        if (op == 8) { free(pl); ws_frame(w, 8, NULL, 0); return -1; }   /* fermeture */
        if (op == 9) { ws_frame(w, 10, pl, (int)n); free(pl); continue; }   /* ping -> pong */
        if (op == 10) { free(pl); continue; }
        if (op <= 2) {                                    /* texte, binaire ou suite d'un fragment */
            if (!tooLong && len + (int)n < cap) { memcpy(buf + len, pl, (size_t)n); len += (int)n; }
            else tooLong = 1;
        }
        free(pl);
        if (fin && op <= 2) {
            if (tooLong) return 0;                        /* ligne beaucoup trop longue : on jette */
            buf[len] = 0;
            return len;
        }
    }
}

void mac_ws_close(MacWs *w) {                             /* coupe : le recv en cours echoue */
    if (!w) return;
    atomic_store(&w->closed, 1);
    shutdown(w->fd, SHUT_RDWR);
}

void mac_ws_free(MacWs *w) {
    if (w) ws_destroy(w);
}
