/* BOURDONNEMENT - etat global de l'application (ce que tout le monde doit connaitre tot)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

enum { APP_MENU, APP_GAME, APP_EN_LIGNE };
enum { ACT_NONE, ACT_JOUER, ACT_REPRENDRE, ACT_MENU, ACT_QUITTER, ACT_PLEIN_ECRAN, ACT_PARAM, ACT_FERMER_PARAM, ACT_MAJ,
       ACT_EN_LIGNE, ACT_CREER, ACT_REJOINDRE, ACT_REJOINDRE_CODE, ACT_LANCER, ACT_QUITTER_SALON, ACT_RETOUR };
static int pendingArg;                                    /* argument de l'action (indice du salon...) */

static int appState = APP_MENU;
static int pendingAction;                                 /* pose par un bouton, traite par do_action apres l'image */
static double nowT;                                       /* horloge absolue (s) */
static float gameT;                                       /* temps de jeu (s), avance par game_tick */
static float fps;
static int fullscreen;

/* interface en mode immediat (curseur, clic) */
static int uiMX, uiMY, uiClick, uiDown, uiDrag, uiFocus;

/* ligne d'etat du menu (mise a jour, micro...) */
static char statut[128];
static double statutT;
static void set_status(const char *s) { snprintf(statut, sizeof statut, "%s", s); statutT = nowT; }

static int settingsOpen;                                  /* ecran PARAMETRES ouvert (menu ou pause) */
static int flashsOn = 1;                                  /* flashs et images subliminales (photosensibilite) */
static char cfgPath[1024];                                /* bourdonnement.ini */
