/* BOURDONNEMENT - TOUS les #define de reglage : joueur, camera, lampe, monde, lumiere, monstre, peur, son, entrees
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* --- joueur (m/s, s) --- */
#define MARCHE_V          2.1f    /* marche */
#define COURSE_V          5.4f    /* course (tant qu'il reste de l'endurance) */
#define ACCROUPI_V        1.2f    /* accroupi : lent et presque silencieux */
#define ACCEL_K           7.0f    /* la vitesse rejoint la vitesse voulue a ce taux (1/s) quand on pousse */
#define FREIN_K           11.0f   /* ... et quand on lache (on s'arrete un peu plus vite qu'on ne demarre) */
#define ENDURANCE_MAX     6.0f    /* secondes de course a pleine endurance */
#define ENDURANCE_RECUP   10.0f   /* secondes pour la remplir de zero */
#define ESSOUFFLE_SEUIL   0.3f    /* a bout de souffle : plus de course avant d'avoir repris cette part */
#define RAYON_JOUEUR      0.3f    /* rayon de collision (plan XZ) */
#define OEIL_DEBOUT       1.65f   /* hauteur des yeux */
#define OEIL_ACCROUPI     1.0f
#define OEIL_SOUS_LIT     0.12f   /* sous un lit, a plat ventre */
#define ARMOIRE_REGARD    0.55f   /* dans une armoire : on tourne la tete de +/- tant (radians) */
#define BRUIT_ARMOIRE     0.35f   /* ouvrir / fermer une armoire */
#define OEIL_K            8.0f    /* vitesse de passage debout <-> accroupi (1/s) */
#define PENCHE_DIST       0.38f   /* se pencher : decalage lateral de la tete (m) */
#define PENCHE_ROULIS     11.0f   /* ... et inclinaison (degres) */
#define PENCHE_K          9.0f    /* vitesse de la penche (1/s) */
#define ROULIS_STRAFE     1.6f    /* inclinaison (degres) a pleine vitesse laterale */

/* --- bruit emis par le joueur (0 a 2) : ce que le monstre entend --- */
#define BRUIT_ACCROUPI    0.08f
#define BRUIT_MARCHE      0.40f
#define BRUIT_COURSE      1.00f
#define BRUIT_SOUFFLE     0.30f   /* a bout de souffle */
#define BRUIT_LAMPE       0.25f   /* clic de la lampe (impulsion) */
#define BRUIT_RAMASSER    0.35f
#define BRUIT_CRI         2.00f
#define BRUIT_DECROIT     2.5f    /* une impulsion de bruit retombe a ce taux (1/s) */

/* --- camera --- */
#define FOV_V             70.0f   /* champ de vision vertical (degres) */
#define FOV_COURSE        7.0f    /* + en courant */
#define BOB_AMP           0.040f  /* balancement de tete : amplitude verticale (m) a la course */
#define BOB_LAT           0.018f  /* ... et laterale */
#define PAS_DIST_MARCHE   0.75f   /* distance entre deux pas (m) */
#define PAS_DIST_COURSE   1.05f
#define CAM_NEAR          0.05f
#define CAM_FAR           60.0f

/* --- lampe torche --- */
#define LAMPE_BATTERIE    420.0f  /* secondes d'autonomie (rechargee a chaque niveau) */
#define LAMPE_FAIBLE      0.18f   /* sous cette part, elle clignote */
#define LAMPE_ANGLE       24.0f   /* demi-angle du faisceau (degres) */
#define LAMPE_PUISSANCE   1.0f
#define LAMPE_PORTEE_FOG  0.11f   /* brouillard noir de la passe de lampe : plus grand = portee plus courte */
#define LAMPE_RETARD_K    12.0f   /* le faisceau suit le regard avec un leger retard (1/s) */
#define LAMPE_MONSTRE_D   9.0f    /* le monstre plus pres que ca fait gresiller la lampe */

/* --- monde --- */
#define HAUT_PLAFOND      2.8f    /* hauteur sous plafond */
#define NEON_PAS_CASES    3       /* un neon automatique toutes les 3 cases (x et z) */
#define LM_DENS           8       /* texels de lightmap par metre */
#define LM_ATLAS          1024    /* cote de l'atlas des lightmaps */
#define NEON_I            2.3f    /* puissance d'un neon (eclairement direct) */
#define NEON_PORTEE       7.5f    /* au-dela, un neon n'eclaire plus (precalcul) */
#define LAMPE_MEUBLE_I    1.7f    /* lampe de chevet, veilleuse, lanterne : lumiere chaude */
#define LAMPE_MEUBLE_PORTEE 6.0f
#define REBOND_K          0.30f   /* lumiere renvoyee par les murs jaunes (rebond approche) */
#define AMBIANCE_MIN      0.022f  /* le noir n'est jamais tout a fait noir */
#define FOG_DENSITE       0.22f   /* brouillard du niveau 1 (les suivants sont plus denses) */
#define CARTE_GEN_MIN     26      /* niveaux generes : cote de la carte au niveau 2 (+3 par niveau, 46 au plus) */

/* --- le Rodeur (monstre aveugle, chasse au bruit) --- */
#define RODEUR_ERRE_V     1.35f   /* vitesse quand il erre */
#define RODEUR_ENQUETE_V  2.7f    /* quand il va voir un bruit */
#define RODEUR_CHASSE_V   4.95f   /* quand il te poursuit : la course le distance... tant que l'endurance tient */
#define RODEUR_OUIE       15.0f   /* portee d'ecoute pour un bruit de 1 (course) ; derriere un mur : moitie */
#define RODEUR_PRISE      0.75f   /* a cette distance, il t'attrape */
#define PROTECTION_DEPART 3.0f    /* debut de niveau : personne ne peut etre attrape pendant ces secondes */
#define REAPPARITION_DELAI 5.0f  /* mort en ligne : au bout de ces secondes, [ESPACE] fait reapparaitre au depart */
#define SPECTATEUR_RECUL  2.2f    /* mort en ligne : la camera suit l'autre a cette distance, derriere lui */
#define RODEUR_OUBLI      3.5f    /* sans t'entendre pendant ce temps, il abandonne la poursuite */
#define RODEUR_ERRE_RMIN  6.0f    /* il erre dans un anneau autour du joueur : se balade "a cote" */
#define RODEUR_ERRE_RMAX  17.0f
#define RODEUR_DEPART_MIN 22.0f   /* il apparait au moins a cette distance du depart */
#define RODEUR_NIVEAU_K   0.08f   /* +8 % de vitesse par niveau */

/* --- le Locataire (n'avance que dans le noir) --- */
#define LOCATAIRE_V       3.1f    /* vitesse quand personne ne l'eclaire */
#define LOCATAIRE_LUMIERE 0.30f   /* sous un neon plus lumineux que ca, il est fige */
#define LOCATAIRE_REVEIL  14.0f   /* il ne bouge qu'apres ce temps (s) dans le niveau */
#define LAMPE_PORTEE      15.0f   /* le faisceau fige le Locataire / attire la Rampante jusqu'a cette distance */

/* --- la Rampante (fonce sur la lumiere) --- */
#define RAMPANTE_ERRE_V   2.6f    /* elle trotte par a-coups */
#define RAMPANTE_CHASSE_V 6.3f    /* plus rapide que toi : la seule issue, c'est le noir */
#define RAMPANTE_VUE      22.0f   /* elle voit une lampe allumee jusque-la (ligne de vue) */
#define RAMPANTE_PROCHE   7.0f    /* plus pres, elle sent la lampe meme si elle ne la voit pas */
#define RAMPANTE_OUBLI    1.4f    /* lampe eteinte depuis ce temps : elle perd ta trace */

/* --- sol et pieges --- */
#define BRUIT_EAU         0.85f   /* pas dans une flaque (accroupi : x 0,4) */
#define BRUIT_VERRE       1.25f   /* pas sur du verre (accroupi : x 0,4) */
#define EAU_FREIN         0.8f    /* on avance moins vite dans l'eau */
#define MACHOIRE_T        3.0f    /* immobilise (s) ; chaque appui sur E en retire MACHOIRE_E */
#define MACHOIRE_E        0.35f
#define BRUIT_MACHOIRE    1.9f
#define ALARME_T          6.0f    /* un faux fusible sonne pendant ce temps */
#define BRUIT_ALARME      1.6f

/* --- micro : crier dans le micro, c'est faire du bruit dans le jeu --- */
#define MICRO_GAIN_DEFAUT 4.0f    /* sensibilite (reglable dans PARAMETRES) */
#define MICRO_SEUIL       0.12f   /* niveau (apres gain) en dessous duquel on chuchote : rien n'est entendu */
#define MICRO_PLAGE       0.25f   /* au-dessus du seuil, + MICRO_PLAGE = un bruit de 1 (comme courir) */
#define MICRO_MAX         2.0f    /* un cri : aussi fort que crier avec Espace */

/* --- coeur --- */
#define BPM_REPOS         66.0f
#define BPM_MAX           192.0f
#define BPM_COURSE        38.0f   /* + en courant */
#define BPM_TENSION       85.0f   /* + a tension maximale (monstre tout pres, poursuite) */
#define BPM_SURSAUT       45.0f   /* pic sur un sursaut (piege, apparition, alarme) */

/* --- peur, screamers --- */
#define SUBLIMINAL_MIN    35.0f   /* images subliminales : au moins tant de secondes entre deux */
#define SUBLIMINAL_MAX    80.0f
#define SCREAMER_MORT_T   2.3f    /* duree du screamer de mort */
#define APPARITION_T      0.55f   /* duree d'une apparition dans le faisceau */
#define APPARITION_MIN    95.0f   /* au moins tant de secondes entre deux apparitions */
#define APPARITION_MAX    200.0f
#define FAUX_BRUIT_MIN    40.0f   /* bruits lourds derriere toi (faux departs) */
#define FAUX_BRUIT_MAX    90.0f
#define COEUR_DIST        13.0f   /* battements de coeur quand le monstre est plus pres que ca */

/* --- mini-carte --- */
#define MINICARTE_VUE     7       /* on decouvre les cases en vue a moins de tant de cases */
#define MINICARTE_TAILLE  170.0f  /* cote (pixels) */
#define MINICARTE_CASES   26      /* cases visibles de cote */

/* --- objectif --- */
#define FUSIBLES_N        3       /* fusibles a trouver par niveau */
#define RAMASSER_DIST     1.5f
#define ASC_PORTE_T       1.3f    /* duree d'ouverture / fermeture des portes d'ascenseur */

/* --- son --- */
#define BOURDON_VOL_MENU  0.20f   /* bourdonnement des neons dans les menus */
#define BOURDON_VOL_JEU   0.30f   /* ... en jeu */
#define PAS_VOL           0.50f

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
