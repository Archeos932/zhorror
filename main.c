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

#include "src/jeu/etat.c"                        /* etat de l'application, actions de menu, curseur, ligne d'etat */
#include "src/son/micro.c"                       /* le micro : ce que tu dis, le monstre l'entend */
#include "src/entrees/actions.c"                 /* touches physiques, actions A_*, touches par defaut */
#include "src/entrees/saisie.c"                  /* input_poll, capture de la souris, fenetre (winW, winH) */

#include "src/monde/carte.c"                     /* grille, neons, ascenseur, fusibles, collisions, rayons */
#include "src/monde/generation.c"                /* niveaux generes par graine */
#include "src/monde/niveau_essai.c"              /* niveau 1 fait a la main */
#include "src/son/son3d.c"                       /* son 3D : distance, gauche / droite, etouffe derriere les murs */

#include "src/joueur/mouvement.c"                /* joueur : marche, course, penche, bruit, lampe */
#include "src/reseau/joueurs.c"                  /* les joueurs de la partie, cibles des monstres */
#include "src/monstres/monstres.c"               /* Rodeur (bruit), Locataire (noir), Rampante (lumiere) */
#include "src/jeu/textes.c"                      /* textes de l'histoire (genere par outils/textes.py) */
#include "src/jeu/histoire.c"                    /* cartons tapes a la machine, messages radio */
#include "src/jeu/niveaux.c"                     /* niveaux, monstres par niveau, pieges, fusibles, ascenseur */
#include "src/jeu/screamers.c"                   /* peur : screamers, apparitions, coeur, drone */
#include "src/reseau/ws.c"                       /* WebSocket vers le serveur de salons (Cloudflare) */
#include "src/reseau/reseau.c"                   /* salons, etats, instantanes de l'hote */
#include "src/jeu/boucle.c"                      /* game_tick (un pas de simulation), ambiance */

#include "src/rendu/base.c"                      /* polices, camera, passage en 2D */
#include "src/rendu/textures.c"                  /* textures fabriquees : papier peint, moquette, plafond, lampe */
#include "src/rendu/eclairage.c"                 /* faces du monde, lightmaps des neons (precalcul) */
#include "src/rendu/monde.c"                     /* dessin du monde : multitexture, neons fous, lampe projetee */
#include "src/interface/texte.c"                 /* texte et rectangles 2D */
#include "src/rendu/entites.c"                   /* Rodeur, fusibles, ascenseur, tete du screamer */
#include "src/interface/mise_a_jour.c"           /* mise a jour depuis GitHub (branche release) */
#include "src/interface/config.c"                /* bourdonnement.ini : reglages du joueur */
#include "src/interface/menus.c"                 /* ecran titre, pause, incrustation camescope */
#include "src/rendu/render.c"                    /* render() : l'image complete */

#include "src/plateforme/fenetre.c"              /* fenetre, entrees clavier/souris */
#include "src/entrees/manette.c"                 /* manette PS4 / Xbox */

#include "src/tests/tests.c"                     /* captures --shot et tests automatiques --test */

#include "src/plateforme/demarrage.c"            /* point d'entree et boucle principale */
