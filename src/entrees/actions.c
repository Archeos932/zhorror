/* BOURDONNEMENT - touches physiques, actions A_*, touches par defaut (structure reprise de VELOCITY)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

enum { SC_ESC = 0x01, SC_W = 0x11, SC_E = 0x12, SC_Q = 0x10, SC_LCTRL = 0x1D, SC_A = 0x1E, SC_S = 0x1F, SC_D = 0x20,
       SC_F = 0x21, SC_LSHIFT = 0x2A, SC_C = 0x2E, SC_SPACE = 0x39, SC_F11 = 0x57 };
/* touches physiques (scancodes 0-255) + souris (256+) + manette (K_PAD_*) : etat tenu et appui du tick.
   Les codes manette suivent l'ordre des bits de padBtn (manette.c). */
enum { K_LMB = 256, K_RMB, K_MMB, K_X1, K_X2, K_WHEEL,
       K_PAD_CROSS, K_PAD_CIRCLE, K_PAD_SQUARE, K_PAD_TRIANGLE, K_PAD_L1, K_PAD_R1, K_PAD_L2, K_PAD_R2,
       K_PAD_SHARE, K_PAD_OPTIONS, K_PAD_L3, K_PAD_R3, K_PAD_PS, K_PAD_TOUCH,
       K_PAD_UP, K_PAD_DOWN, K_PAD_LEFT, K_PAD_RIGHT,                         /* croix directionnelle */
       K_PAD_LS_UP, K_PAD_LS_DOWN, K_PAD_LS_LEFT, K_PAD_LS_RIGHT,             /* stick gauche pousse */
       K_COUNT };
#define K_PAD_FIRST K_PAD_CROSS
#define PAD_NBTN (K_PAD_LS_UP - K_PAD_FIRST)              /* boutons lus tels quels (le stick gauche est a part) */
static unsigned char keys[K_COUNT], latch[K_COUNT];

/* Actions du jeu, chacune liee a deux touches au plus (0 = aucune). Le jeu ne lit QUE les actions. */
enum { A_AVANCER, A_RECULER, A_GAUCHE, A_DROITE, A_COURIR, A_ACCROUPI, A_LAMPE, A_UTILISER, A_CRIER,
       A_PENCHER_G, A_PENCHER_D, A_COUNT };
static const char *actId[A_COUNT] = { "avancer", "reculer", "gauche", "droite", "courir", "accroupi", "lampe", "utiliser",
                                      "crier", "pencher_gauche", "pencher_droite" };
static const char *actLabel[A_COUNT] = { "Avancer", "Reculer", "Gauche", "Droite", "Courir (maintenir)", "S'accroupir",
                                         "Lampe torche", "Utiliser / ramasser / lire", "Crier", "Se pencher a gauche",
                                         "Se pencher a droite" };
static const int bindDefault[A_COUNT][2] = {
    { SC_W, 0 }, { SC_S, 0 }, { SC_A, 0 }, { SC_D, 0 }, { SC_LSHIFT, 0 }, { SC_LCTRL, SC_C }, { SC_F, K_RMB },
    { SC_E, K_LMB }, { SC_SPACE, 0 }, { SC_Q, 0 }, { 0x13 /* R */, 0 },
};
static int binds[A_COUNT][2];
/* manette : une touche par action. Stick gauche = deplacement, stick droit = regard. */
static const int padBindDefault[A_COUNT] = {
    K_PAD_LS_UP, K_PAD_LS_DOWN, K_PAD_LS_LEFT, K_PAD_LS_RIGHT, K_PAD_L3, K_PAD_CIRCLE, K_PAD_R1,
    K_PAD_CROSS, K_PAD_TRIANGLE, K_PAD_L1, K_PAD_R2 };
static int padBinds[A_COUNT];
static float padSens = PAD_SENS_DEFAULT;                  /* visee au stick droit : vitesse maxi (rad/s) */
static int padInvertY;                                    /* stick droit : axe vertical inverse */
static int padOn;                                         /* une manette est branchee (manette.c) */
static char padName[64];
static int waitPad = -1;                                  /* reaffectation d'un bouton de manette en attente */
static int padCursor;                                     /* curseur des menus pilote au stick */
static unsigned char act[A_COUNT], actP[A_COUNT];          /* tenu / appuye pendant ce tick */
static unsigned char botHeld[A_COUNT], botPress[A_COUNT];  /* ce que les bots (tests) "appuient" */

static int invertY;
