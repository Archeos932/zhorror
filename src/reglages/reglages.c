/* BOURDONNEMENT - TOUS les #define de reglage : joueur, camera, son, entrees
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* --- joueur (m/s, s) --- */
#define MARCHE_V          1.6f    /* marche */
#define COURSE_V          4.2f    /* course (tant qu'il reste de l'endurance) */
#define ACCROUPI_V        0.9f    /* accroupi : lent et silencieux */
#define ACCEL_K           10.0f   /* la vitesse rejoint la vitesse voulue a ce taux (1/s) */
#define ENDURANCE_MAX     6.0f    /* secondes de course a pleine endurance */
#define ENDURANCE_RECUP   10.0f   /* secondes pour la remplir de zero */
#define ESSOUFFLE_SEUIL   0.3f    /* a bout de souffle : plus de course avant d'avoir repris cette part */
#define RAYON_JOUEUR      0.3f    /* rayon de collision (plan XZ) */
#define OEIL_DEBOUT       1.65f   /* hauteur des yeux */
#define OEIL_ACCROUPI     1.0f
#define OEIL_K            8.0f    /* vitesse de passage debout <-> accroupi (1/s) */

/* --- camera --- */
#define FOV_V             70.0f   /* champ de vision vertical (degres) */
#define BOB_AMP           0.035f  /* balancement de tete : amplitude verticale (m) a la course */
#define PAS_DIST_MARCHE   0.75f   /* distance entre deux pas (m) */
#define PAS_DIST_COURSE   1.05f
#define CAM_NEAR          0.05f
#define CAM_FAR           80.0f

/* --- salle de test (etape 0, remplacee par la generation a l'etape 2) --- */
#define SALLE_DEMI        10.0f   /* demi-cote de la salle (m) */
#define SALLE_HAUT        2.8f    /* hauteur sous plafond */

/* --- son --- */
#define BOURDON_VOL_MENU  0.22f   /* bourdonnement des neons dans les menus */
#define BOURDON_VOL_JEU   0.38f   /* ... en jeu */
#define PAS_VOL           0.55f

/* --- entrees (repris de VELOCITY) --- */
#define WHEEL_QUIET        0.15    /* molette : repos (s) qui separe deux gestes ; une rafale = un seul changement */
#define WHEEL_COOLDOWN     0.30    /* molette : delai minimal (s) entre deux changements */
#define PAD_SENS_DEFAULT   3.2f    /* manette : visee au stick droit, vitesse maxi (rad/s, ~180 deg/s) */
#define PAD_PITCH_K        0.7f    /* ...l'axe vertical tourne moins vite */
#define PAD_DEADZONE       0.12f   /* zone morte des sticks (part de la course) */
#define PAD_STICK_PRESS    0.4f    /* stick gauche : pousse au-dela = touche K_PAD_LS_* appuyee */
#define PAD_TRIGGER_PRESS  0.35f   /* L2 / R2 : enfonce au-dela = appuye */
#define PAD_CURSOR_SPEED   1100.0f /* menus : curseur au stick gauche (pixels/s a fond) */
#define PAD_SCAN_T         2.0f    /* sans manette : on recherche toutes les 2 s */
