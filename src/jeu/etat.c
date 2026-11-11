/* BOURDONNEMENT - etat global de l'application (ce que tout le monde doit connaitre tot)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

enum { APP_MENU, APP_GAME };
enum { ACT_NONE, ACT_JOUER, ACT_REPRENDRE, ACT_MENU, ACT_QUITTER, ACT_PLEIN_ECRAN };

static int appState = APP_MENU;
static int pendingAction;                                 /* pose par un bouton, traite par do_action apres l'image */
static double nowT;                                       /* horloge absolue (s) */
static float gameT;                                       /* temps de jeu (s), avance par game_tick */
static float fps;
static int fullscreen;

/* interface en mode immediat (curseur, clic) */
static int uiMX, uiMY, uiClick, uiDown, uiDrag, uiFocus;
