/*
 * BOURDONNEMENT - jeu d'horreur cooperatif (2 a 3 joueurs) dans les backrooms, en C pur.
 * Win32 + OpenGL 1.1 + waveOut, aucune dependance externe (meme ossature que VELOCITY).
 * macOS : couche plateforme dans src/plateforme/mac/ (Cocoa, AudioQueue).
 * Linux : meme API, src/plateforme/linux/linux_platform.c (X11 + GLX, ALSA, FreeType).
 *
 * Ce fichier est la table des matieres du jeu : il inclut les morceaux de src/ DANS CET ORDRE et le tout
 * forme une seule unite de compilation (build.bat, build_mac.sh et build_linux.sh ne compilent que main.c).
 * Une fonction doit etre definie (ou declaree) dans un morceau place plus haut que celui qui l'utilise.
 * Plan : docs/PLAN.md ; histoire : docs/SCENARIO.md.
 */

#include "src/noyau/entete.c"                    /* includes systeme (Win32 ou couche macOS/Linux), PI et TICK */
#include "src/noyau/maths.c"                     /* vecteurs 3D, bornes, aleatoire */

#include "src/reglages/reglages.c"               /* TOUS les #define de reglage */

#include "src/son/audio.c"                       /* mixeur waveOut / AudioQueue / ALSA, voix, boucles */
#include "src/son/sons.c"                        /* synthese des bruitages : bourdon des neons, pas, gresillement */

#include "src/jeu/etat.c"                        /* etat de l'application, actions de menu, curseur */
#include "src/entrees/actions.c"                 /* touches physiques, actions A_*, touches par defaut */
#include "src/entrees/saisie.c"                  /* input_poll, capture de la souris, fenetre (winW, winH) */

#include "src/monde/salle_test.c"                /* etape 0 : salle de test, neons, collisions */
#include "src/joueur/mouvement.c"                /* joueur : marche, course, endurance, accroupi, pas */
#include "src/jeu/boucle.c"                      /* game_tick (un pas de simulation), ambiance */

#include "src/rendu/base.c"                      /* polices, camera, passage en 2D */
#include "src/interface/texte.c"                 /* texte et rectangles 2D */
#include "src/interface/menus.c"                 /* ecran titre, pause, incrustation camescope */
#include "src/rendu/render.c"                    /* render() : l'image complete */

#include "src/plateforme/fenetre.c"              /* fenetre, entrees clavier/souris */
#include "src/entrees/manette.c"                 /* manette PS4 / Xbox */

#include "src/tests/tests.c"                     /* captures --shot et tests automatiques --test */

#include "src/plateforme/demarrage.c"            /* point d'entree et boucle principale */
