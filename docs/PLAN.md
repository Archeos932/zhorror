# BOURDONNEMENT — plan détaillé

Jeu d'horreur coopératif **2 à 3 joueurs** à la première personne, dans des « backrooms » : couloirs jaunes
infinis, néons qui grésillent, moquette humide, et des choses qui y vivent. Écrit en **C pur**, sur le modèle
technique de **VELOCITY** (`~/GitHub/velocity`), dont on reprend l'ossature et une grande partie du code.

Le scénario complet (histoire, textes, dialogues, fins) est dans **[`SCENARIO.md`](SCENARIO.md)**.

---

## 1. Ce qu'on reprend de VELOCITY (et comment)

VELOCITY est un jeu de tir 3D en C **sans aucune dépendance** : une seule unité de compilation (`main.c` inclut les
morceaux de `src/` dans l'ordre), Win32/Cocoa/X11 + OpenGL 1.1 en pipeline fixe, son **entièrement synthétisé**
au démarrage, réseau UDP avec un hôte qui fait autorité, lobby en ligne sur Cloudflare. ~27 000 lignes.

| Brique VELOCITY | Fichiers d'origine | Dans BOURDONNEMENT |
|---|---|---|
| Une seule unité de compilation, `main.c` = table des matières | `main.c` | **Identique.** Même règle : un fichier par thème, l'ordre d'inclusion compte. |
| Couche plateforme Windows / macOS / Linux | `src/plateforme/` (`fenetre.c`, `demarrage.c`, `mac/`, `linux/`) | **Copiée telle quelle**, puis allégée (pas de second écran, pas de mise à jour au début). |
| Maths, vecteurs, aléatoire déterministe | `src/noyau/maths.c` | **Copiée.** La graine partagée sert à générer les mêmes niveaux chez tous. |
| Tick fixe 120 Hz + rendu interpolé | `demarrage.c`, `boucle.c` | **Identique.** Tout ce qui fait peur doit être réglé au tick près. |
| Actions `A_*` et touches réaffectables, manette | `src/entrees/` | **Copiées**, nouvelle liste d'actions (lampe, courir, s'accroupir, ramasser, lire, crier, radio). |
| Mixeur audio 32 voix, sons synthétisés (`make_sounds`) | `src/son/audio.c` | **Base du moteur sonore**, étendue : son 3D, réverbération, étouffement derrière les murs. |
| Séquenceur de musique `music.h` (niveau d'intensité 0-8) | `src/son/music.h` | **Réécrit en moteur d'ambiance** (`ambiance.h`) : nappes, bourdon, cordes dissonantes, poursuite. Même idée d'**intensité**, pilotée par le « directeur ». |
| Carte en boîtes AABB, collisions, `ray_world` | `src/monde/carte.c` | **Copiée.** Les rayons servent aussi à la vue des monstres et à l'étouffement du son. |
| Lightmaps précalculées, secteurs, frustum | `src/rendu/eclairage.c`, `monde_secteurs.c` | **Reprises**, mais la lumière vient des **néons du plafond** (et plus du soleil) ; un néon peut s'éteindre → secteur assombri. |
| Brouillard, bloom, grain, `rendu.ini` modifiable sans recompiler (F5) | `src/rendu/ciel_ombres_bloom.c`, `graphismes/` | **Repris** + filtre **VHS** (voir §5). |
| Réseau : hôte id 0, `PK_STATE`/`PK_SNAP`, `EV_*` renvoyés ×3, `NET_VER` | `src/reseau/` | **Repris presque tel quel.** `MAXP` = 3. L'hôte fait autorité sur les **monstres**, les **objets**, les **portes**, la **mort** et le **directeur**. |
| Lobby en ligne, STUN, perçage NAT, relais | `src/reseau/en_ligne.c`, `worker/` | **Copié** : jouer à distance entre amis dès le début. |
| Navigation des bots (graphe auto, plus court chemin) | `src/bots/navigation.c` | **Pour les monstres.** Les bots `--bot host/join` restent pour tester le réseau. |
| Ennemis du mode solo | `src/solo/ennemis_vagues.c` | Remplacé par `src/monstres/` (un fichier par créature). |
| Texte (atlas de police CP1252) | `src/interface/texte.c` | **Repris** pour sous-titres, notes, radio. Pas d'accents dans les chaînes C (`\xE9`). |
| Tests `--test`, captures `--shot`, journal `--log` | `src/tests/tests.c` | **Repris dès le premier jour.** Scènes de capture : `couloir`, `noir`, `lampe`, `vhs`, `locataire`… |
| Builds `build.bat`, `build_mac.sh`, `build_linux.sh`, CI | racine, `.github/` | **Copiés**, renommés. |

**Ce qu'on jette** : tout le mouvement nerveux (bhop, wall-run, grappin, dash), les 13 armes, parry, pièces,
compétences, jauge de style, boutique, modes PvP, mode COURSE, cartes Arène/Citadelle/Oasis.

### Architecture cible (`main.c`)

```
src/noyau/        entete.c, maths.c                           (copiés)
src/reglages/     reglages.c        tous les #define : vitesse, peur, lampe, monstres, directeur
src/son/          audio.c           mixeur + son 3D + reverb + filtre passe-bas par voix
                  sons.c            synthese des bruitages (pas, neons, portes, screamers…)
                  ambiance.h        moteur d'ambiance (remplace music.h)
src/monde/        carte.c           AABB, collisions, rayons                (copie)
                  generation.c      niveaux proceduraux par graine (salles + couloirs)
                  niveaux.c         definition de chaque niveau (palette, regles, salles fixes)
                  objets.c          portes, interrupteurs, fusibles, notes, cassettes, radio
src/entrees/      actions.c, manette.c
src/joueur/       mouvement.c       marche, course + endurance, accroupi, se pencher, balancement de tete
                  lampe.c           lampe torche : batterie, clignotement, cone
                  peur.c            jauge de peur / sante mentale, hallucinations
src/monstres/     monstres.c        structure commune, vue, ouie, etats (errer, guetter, chasser, fuir)
                  locataire.c, echo.c, foule.c, mere.c, nageur.c, gardien.c
src/jeu/          directeur.c       tension, budget de peur, choix des evenements (hote)
                  screamers.c       declencheurs et mise en scene des screamers
                  histoire.c        progression du scenario, objectifs, declencheurs de texte
                  boucle.c          game_tick
src/reseau/       etat_partage.c, protocole.c, hote.c, client.c, en_ligne.c   (repris)
src/rendu/        base.c, textures.c (procedurales : moquette, papier peint, beton, carrelage)
                  eclairage.c       lightmaps des neons
                  monde_secteurs.c  (repris)
                  monstres_rendu.c  silhouettes, maillages simples, yeux
                  postfx.c          VHS, aberration chromatique, grain, vignette, flou de peur
                  render.c
src/interface/    texte.c, menus.c, sous_titres.c, notes.c (lecture plein ecran), hud.c (quasi vide)
src/bots/         navigation.c (pour monstres et bots de test), bot.c (bots de test)
src/tests/        tests.c
src/plateforme/   (copiee)
```

---

## 2. Boucle de jeu

1. **Explorer** un niveau généré (même graine chez tous) en groupe… ou séparés, car certaines énigmes l'imposent.
2. **Trouver** les objets d'objectif (fusibles, pièces de la radio, clé de service) et les **textes** (notes,
   cassettes de Noé, messages radio) qui racontent l'histoire.
3. **Survivre** aux créatures : chacune a une règle à comprendre (voir §4). Aucune arme. On peut seulement se
   cacher, fuir, éteindre/allumer sa lampe, faire du bruit ailleurs, fermer des portes.
4. **Sortir** du niveau par la porte de sortie → niveau suivant. Les morts reviennent au niveau suivant…
   **changés** (voir « Revenants », §3.4).

Durée visée : 60 à 90 min pour une partie complète (prologue + 4 niveaux + final). Sauvegarde au début de
chaque niveau (graine + objets + notes trouvées).

---

## 3. Systèmes de jeu

### 3.1 Joueur (`joueur/mouvement.c`)
- Marche 1,6 m/s, course 4,2 m/s (endurance 6 s, récupération 10 s, **souffle audible** quand elle est vide),
  accroupi 0,9 m/s (silencieux), se pencher (Q/E) pour regarder à un angle.
- Balancement de tête et pas synthétisés selon le sol (moquette mouillée, béton, flaque, carrelage, grille).
- Pas de barre de vie : **2 coups = mort** (le premier laisse la vue rouge et le souffle court pendant 30 s).
- Volume sonore émis par le joueur (`P.bruit`, 0 à 1) : ce que les monstres entendent.

### 3.2 Lampe torche (`joueur/lampe.c`)
- Cône de lumière (§5), batterie de 4 min, piles trouvées dans le niveau.
- Batterie faible → clignote ; les monstres font aussi **clignoter les lampes** à proximité (avertissement).
- Allumer = voir, mais être vu. C'est l'arbitrage central.

### 3.3 Peur / santé mentale (`joueur/peur.c`)
- Jauge 0-100, monte : seul (aucun coéquipier à moins de 8 m), dans le noir, après un screamer, en voyant un
  monstre, en lisant certaines notes. Descend : près des autres, sous un néon qui marche.
- Effets progressifs **locaux à chaque joueur** :
  - 30 : acouphène léger, le bourdonnement des néons s'amplifie ;
  - 50 : grain VHS plus fort, bords de l'écran qui respirent, pas fantômes derrière soi ;
  - 70 : **hallucinations** : silhouettes en périphérie, un coéquipier qui apparaît là où il n'est pas,
    chuchotements avec son pseudo (synthèse : bruit filtré en formants), faux sous-titres de la radio ;
  - 90 : la créature du niveau est attirée vers vous.
- **Clé du multijoueur** : chaque client fait son propre rendu, donc le jeu peut **mentir à un seul joueur**.
  L'hôte envoie des événements ciblés (`EV_SCARE` avec `target`) : un joueur voit quelque chose que les autres
  ne voient pas. « Il est là, derrière toi ! — Quoi ? Il n'y a rien. »

### 3.4 Mort et « revenants »
- Mort → caméra de mort (reprise de `camera_mort.c`) qui fixe le monstre, écran VHS qui décroche.
- Le mort devient **spectateur muet** jusqu'à la fin du niveau, mais peut faire **une chose** : faire grésiller
  la lampe d'un vivant ou faire claquer une porte près de lui (aide ou fausse piste : il ne peut pas parler).
- Au niveau suivant il revient. Si tout le groupe meurt : fin « Niveau Zéro » (§ fins du scénario), retour à la
  sauvegarde du début du niveau.

### 3.5 Le directeur (`jeu/directeur.c`) — le cœur de la peur
Inspiré du principe « directeur d'IA » : l'hôte calcule une **tension** 0-100 par joueur et pour le groupe.
- Phases cycliques : **calme** (60-120 s, on respire, on explore, on lit) → **montée** (bruits, néons qui
  clignotent, pas au loin, ambiance qui monte d'un niveau d'intensité) → **pic** (apparition, poursuite ou
  screamer) → **relâche** (silence presque complet, 20-40 s).
- **Budget de screamers** : au plus 1 « vrai » screamer toutes les 4 min par joueur, jamais deux fois le même de
  suite, jamais pendant une poursuite. Les faux départs (bruit fort sans rien) coûtent moins cher.
- Choisit **qui** cible : le joueur le plus isolé, le plus calme, ou celui qui a le moins eu peur récemment.
- Pilote l'intensité de l'ambiance (`ambiance_niveau(n)`, comme `music_level` de VELOCITY).
- Tout est déterministe à partir de la graine + événements → rejouable dans les tests.

### 3.6 Screamers et événements (`jeu/screamers.c`)
Règle d'or : **la peur vient de l'attente**, le screamer n'est que la ponctuation.

| Type | Exemple | Cible |
|---|---|---|
| Visage plein écran | Au fond d'un couloir après un clignotement, le Locataire est à 30 cm de la caméra, cri + flash + gel de 0,3 s (`hitstop` de VELOCITY) | un joueur |
| Retournement | Le joueur se retourne (yaw > 150° en < 0,4 s) : quelque chose était derrière lui et disparaît dans l'image suivante | un joueur |
| Faux coéquipier | L'Écho prend l'apparence d'un ami ; quand on s'approche, la tête pivote à 180° | un joueur |
| Coupure | Tous les néons du niveau s'éteignent d'un coup, silence total 3 s, puis un seul se rallume… sur la créature | groupe |
| Porte | Une porte fermée derrière soi s'ouvre lentement, grincement | un joueur |
| Radio | La radio crache le **pseudo** du joueur avec une voix déformée | un joueur |
| Cassette | Une cassette de Noé montre… le joueur, filmé de dos, il y a 10 secondes | un joueur |
| Faux plantage | Écran figé, « VHS TRACKING ERROR », son en boucle, puis une main sur l'objectif | groupe (rare, 1 par partie) |

Mise en scène d'un screamer : coupure de l'ambiance 0,5 s avant (le silence prévient sans prévenir) →
image + son à fort volume (compresseur doux `tanhf` déjà dans le mixeur) → flash blanc, secousse caméra,
aberration chromatique à fond → retour lent du son. Réglage `screamers=0/1/2` (doux / normal / extrême) et
avertissement photosensibilité au démarrage.

### 3.7 Objets et énigmes (`monde/objets.c`)
- **Coopératives** : un levier qu'il faut tenir pendant qu'un autre court ouvrir la porte ; deux interrupteurs
  éloignés à presser en même temps ; un joueur guide un autre dans le noir en regardant les caméras de
  surveillance ; trois fusibles répartis loin les uns des autres.
- **À 2 joueurs** les énigmes à 3 deviennent possibles à 2 (un levier qui reste coincé 10 s) : `nb_joueurs`
  règle chaque énigme.
- **Textes** : notes de papier (lecture plein écran, papier jauni, écriture manuscrite = police + tremblement),
  cassettes VHS de Noé (on les regarde sur des télés trouvées : petite scène pré-rendue avec le moteur),
  messages radio (sous-titres avec effet machine à écrire).

### 3.8 Communication
- Pas de chat vocal au début (capture micro sur 3 OS = lourd). On joue en appel Discord à côté, et le jeu le
  sait : **aucune information n'est donnée à l'écran que les joueurs ne pourraient pas se dire**.
- Dans le jeu : **crier** (touche) → son fort entendu par tous (et par les monstres !), **talkie-walkie**
  ramassé au niveau 1 : messages courts prédéfinis (« ICI », « VIENS », « COURS », « J'AI TROUVE ») qui passent
  par la radio… que l'Écho peut aussi imiter.
- Plus tard (phase 7, option) : chat vocal de proximité dans le jeu (capture micro, codec simple, transmis
  dans `PK_STATE`) ; l'Écho rejouerait de vrais extraits de voix des joueurs. Effrayant mais coûteux.

---

## 4. Les créatures (`src/monstres/`)

Toutes partagent `monstres.c` : position, état (`ERRER`, `GUETTER`, `ENQUETER`, `CHASSER`, `FUIR`,
`CACHE`), vue (cône + `ray_world`, gêné par le noir), ouïe (`P.bruit` atténué par la distance et les murs),
chemin par le graphe de `navigation.c`. **L'hôte les simule**, les clients reçoivent leur état dans `PK_SNAP`
(position, direction, état, pose d'animation) et l'interpolent comme les joueurs distants de VELOCITY.

| Créature | Niveau | Règle | Comment survivre |
|---|---|---|---|
| **Le Locataire** | 0 | Grand homme maigre en chemise de bureau jaunie, tête penchée à angle droit. **Ne bouge que quand les néons s'éteignent.** Chaque clignotement = il est plus près. | Rester sous des néons stables, ne pas traîner dans les zones qui clignotent, ne jamais le perdre de vue pendant un noir. |
| **L'Écho** | 0 → final | Imitateur. Prend l'apparence et le pseudo d'un coéquipier, imite ses messages radio. Détails faux : pas de balancement de tête, lampe éteinte, pseudo qui tremble. | Vérifier (« dis ICI deux fois »), se méfier de celui qui ne répond pas. Il ne tue pas : il **attire** vers les autres créatures. |
| **La Foule** | 1 | Dizaines de silhouettes debout dans les entrepôts noirs. Immobiles quand la lampe les éclaire ; dans le noir, elles avancent. Chaque fois qu'on rallume, elles sont plus près. | Garder la lampe sur elles en reculant ; à plusieurs, se répartir les directions. La batterie devient vitale. |
| **La Mère** | 2 | Énorme, aveugle, rampe dans les tuyaux. Entend tout : course, cri, objet qui tombe, porte claquée. | S'accroupir, marcher sur les grilles silencieuses, lancer un objet pour l'attirer ailleurs, se cacher dans les casiers (respiration retenue = mini-jeu). |
| **Le Nageur** | 3 | Forme pâle sous l'eau des piscines. Ne sort pas de l'eau, mais l'eau monte quand on reste trop longtemps. | Traverser vite, rester sur les bords, ne pas regarder l'eau (le regarder trop longtemps l'appelle). |
| **Le Gardien** | final | Tout ce qui précède à la fois, dans la copie déformée du centre commercial. | Voir le scénario. |

---

## 5. Rendu : faire peur avec OpenGL 1.1

On garde le pipeline fixe de VELOCITY (rapide partout, même sur un « grille-pain »).

- **Néons** : chaque panneau de plafond est une source pour les lightmaps (au lieu du soleil). Les lightmaps sont
  calculées **par néon** dans des canaux séparés par secteur → éteindre un néon = remplacer la lightmap du
  secteur par une version « éteinte » précalculée (deux jeux de lightmaps : allumé / éteint, mélangés par la
  couleur des sommets). Clignotement = mélange qui saute entre les deux, synchronisé au grésillement sonore.
- **Lampe torche** : texture projetée (un disque doux avec des imperfections de réflecteur) via `glTexGen` en
  mode `GL_EYE_LINEAR` sur une unité de texture (OpenGL 1.1 + multitexture déjà détecté par `world_gl_caps`),
  multipliée par l'atténuation. Repli sans multitexture : `GL_LIGHT0` en spot sur un maillage subdivisé.
- **Noir** : `GL_FOG` exponentiel couleur presque noire (jaune sale au niveau 0), distance réglable par niveau.
- **Post-traitement VHS** (`rendu/postfx.c`, à partir du code du bloom et de `render_scale_apply`) :
  scène copiée dans une texture puis redessinée avec : décalage rouge/bleu (aberration chromatique, 3 passes
  avec `glColorMask`), lignes de balayage, bande de « tracking » qui descend, grain (déjà présent), vignette,
  désaturation, légère distorsion en barillet (grille de quads). Incrustation `REC ●` et horodatage
  `OCT 14 2003 03:12 AM` dans le coin. Intensité liée à la peur et au directeur.
- **Textures procédurales** (comme `make_textures`) : papier peint jaune à motifs, moquette tachée, dalles de
  plafond, béton, carrelage de piscine, + `.bmp` remplaçables dans `graphismes/` comme VELOCITY.
- **Créatures** : maillages simples (boîtes allongées, cylindres déformés) + **silhouette noire** : la peur vient
  de la forme et de la pose, pas du détail. Yeux = 2 points lumineux additifs. Membres trop longs, articulations
  à angles impossibles, animation **saccadée** (pose changée tous les 3 ticks au lieu de chaque tick).
- `graphismes/rendu.ini` expose : fog, VHS, grain, aberration, intensité des néons, couleur du jaune, etc. (F5).

---

## 6. Son : la moitié de la peur

Tout reste **synthétisé**, comme VELOCITY (aucun fichier son) — c'est faisable et ça donne un son étrange qui
colle parfaitement au genre.

- **Mixeur** (`audio.c` repris) + par voix : panoramique gauche/droite selon l'angle, volume selon la distance,
  **filtre passe-bas** si un mur est entre la source et l'oreille (`ray_world`), petit retard pour l'oreille
  opposée (effet de tête).
- **Réverbération** (Schroeder : 4 filtres en peigne + 2 passe-tout) réglée par niveau : couloirs moquettés
  (sourd), entrepôts (immense), tuyaux (métallique), piscine (longue et humide).
- **Bruitages** : bourdonnement 120 Hz + harmoniques des néons (le son de base du jeu, jamais tout à fait
  silencieux), grésillement de clignotement, pas par matière, souffle, battements de cœur (montent avec la
  peur), portes, eau, tuyaux qui cognent, respiration de la Mère, chuchotements (bruit dans des filtres de
  formants : « a », « o », « i »), radio (bande passante 300-3000 Hz + saturation + parasites).
- **Screamers** : empilement de scies désaccordées en cluster dissonant + bruit blanc + chute de hauteur +
  saturation forte, 0,8 à 1,5 s. 6 variantes. Précédés d'un trou de silence.
- **Ambiance** (`ambiance.h`, même structure que `music.h` : séquenceur + niveau d'intensité 0 à 8 piloté
  par le directeur) :
  - 0-1 : bourdonnement seul, craquements rares ;
  - 2-3 : nappe grave (sinus désaccordés), cloche lointaine ;
  - 4-5 : cordes dissonantes (scies filtrées, intervalles de seconde mineure / triton), pulsation lente ;
  - 6-7 : poursuite : percussions sourdes, **ton de Shepard** qui monte sans fin ;
  - 8 : tout coupé net. Le silence est le niveau le plus fort.
- Un **thème** par niveau : la boîte à musique du centre commercial (mélodie d'ascenseur désaccordée) revient,
  de plus en plus déformée.
- Comme `musique.exe` de VELOCITY : petit outil `ambiance.exe` pour écouter l'ambiance et les screamers sans le jeu.

---

## 7. Réseau (repris de VELOCITY)

- Même modèle : UDP, hôte = joueur 0, `MAXP = 3`, chaque joueur simule son mouvement et l'envoie (`PK_STATE`
  60/s), l'hôte renvoie l'état de tous (`PK_SNAP` 30-60/s), événements `EV_*` renvoyés 3 fois et dédoublonnés.
- **La géométrie n'est jamais envoyée** : graine + numéro de niveau dans `PK_WELCOME` / `EV_LEVEL`, génération
  déterministe chez chacun (exactement comme `build_level(m)`).
- `PkSnap` ajoute : état des monstres (`MAXMON` = 8 : pos, yaw, état, pose), état des néons (masque de bits),
  portes, objets ramassés, phase du directeur, intensité d'ambiance.
- Nouveaux événements :

| Événement | Émetteur | Effet |
|---|---|---|
| `EV_LEVEL` | hôte | changement de niveau (graine, numéro) |
| `EV_USE` | client → hôte | utiliser porte / levier / objet ; l'hôte arbitre (premier arrivé) |
| `EV_PICK` | hôte | objet ramassé par X |
| `EV_SCARE` | hôte | screamer / hallucination, **`target` = un seul joueur** ou tous |
| `EV_LIGHT` | hôte | néons de la zone N : allumés / clignotent / éteints |
| `EV_NOISE` | client | bruit émis (cri, objet lancé, chute) : nourrit l'ouïe des monstres |
| `EV_TEXT` | hôte | ligne de radio / sous-titre (index dans la table des textes) |
| `EV_DEATH` | hôte | mort de X tué par la créature Y |
| `EV_RADIO` | client | message de talkie-walkie prédéfini |

- **Lobby en ligne** et relais repris de `worker/` : partie privée avec un **code** de 4 lettres.
- Joueur qui se déconnecte : son personnage reste figé là… et l'Écho en prend parfois la place.

---

## 8. Texte et histoire à l'écran (`interface/sous_titres.c`, `notes.c`, `jeu/histoire.c`)

- **Table unique des textes** (`src/jeu/textes.c`) : chaque ligne = id, locuteur, texte, durée, son associé.
  Tous les textes en français **sans accents dans le source C** (`\xE9` etc.) comme VELOCITY, ou un script de
  conversion UTF-8 → CP1252 à la compilation.
- Sous-titres : en bas, effet machine à écrire, couleur par locuteur (radio = vert pâle, Noé = blanc cassé,
  joueurs = leur couleur).
- **Cartons** plein écran entre les niveaux (lettres blanches sur noir, grésillement VHS) : voir SCENARIO.md.
- Notes : lecture plein écran qui met le jeu… **pas en pause** (le monde continue derrière, son étouffé).
- `histoire.c` : machine à états du scénario : déclencheurs (entrer dans une zone, ramasser un objet, temps,
  peur), drapeaux sauvegardés, choix de fin.

---

## 9. Étapes de développement

Chaque étape finit par un jeu **lançable** et vérifié (règles de VELOCITY : zéro avertissement, `--test`
vert, captures `--shot` regardées, partie bot hôte + bot client).

| # | Étape | Contenu | Vérification |
|---|---|---|---|
| 0 | **Squelette** | Copier plateforme, noyau, entrées, audio, texte, réseau de VELOCITY ; `main.c` nettoyé ; builds 3 OS ; CLAUDE.md du projet | Compile sans avertissement sur Mac, fenêtre noire, `--test` passe |
| 1 | **Un couloir jaune** | Marche/course/accroupi, carte AABB faite à la main, textures procédurales, lightmaps de néons, brouillard | `--shot couloir.bmp` ressemble à des backrooms |
| 2 | **Génération** | `generation.c` : salles + couloirs par graine, sortie atteignable (test), néons placés, zones | `--test` : même graine = même carte ; 1000 graines toutes traversables |
| 3 | **Ambiance** | Son 3D, reverb, bourdonnement, pas, `ambiance.h` niveaux 0-8, lampe torche, filtre VHS | Écoute avec `ambiance.exe` ; `--shot vhs.bmp`, `--shot lampe.bmp` |
| 4 | **Multijoueur** | `MAXP` 3, joueurs distants avec lampe visible, `EV_LEVEL`, `EV_USE`, objets et portes ; lobby | Bot hôte + 2 bots clients 120 s, journaux sans désynchro |
| 5 | **Le Locataire + directeur** | `monstres.c`, `locataire.c`, néons qui clignotent, mort, caméra de mort, directeur, 3 screamers | **Tranche verticale** : prologue + niveau 0 jouables à 2. On y joue vraiment et on note ce qui fait peur. |
| 6 | **Histoire** | Textes, sous-titres, notes, radio, cassettes, cartons, `histoire.c`, sauvegarde | Prologue + niveau 0 racontent l'histoire de bout en bout |
| 7 | **Niveaux 1 à 3** | Foule, Mère, Nageur, Écho ; énigmes coop ; peur et hallucinations ciblées | Partie complète à 3 jusqu'au niveau 3 |
| 8 | **Final et fins** | Centre commercial déformé, Gardien, 3 fins, nouvelle partie + | Les 3 fins atteignables (tests de `histoire.c`) |
| 9 | **Finitions** | Menus d'ambiance, réglages (screamers, VHS, sensibilité), manette, équilibrage du directeur, optimisations « grille-pain » | Partie de test avec des gens qui ne connaissent pas le jeu |
| 10 | Option | Chat vocal de proximité, Écho qui rejoue de vraies voix | — |

**Priorité absolue : l'étape 5.** Si le couloir jaune + le Locataire + le son ne font pas peur à 2, le reste
ne sauvera rien. Tout le réglage passe par `src/reglages/reglages.c` et `graphismes/rendu.ini` pour itérer
vite.

---

## 10. Règles du code (héritées de VELOCITY)

- Une seule unité de compilation ; un fichier par thème ; l'ordre d'inclusion dans `main.c` compte.
- Tous les réglages en `#define` dans `src/reglages/reglages.c`.
- La logique ne lit jamais le clavier : seulement les actions `act[A_*]` / `actP[A_*]`.
- Nouvel événement réseau = `EV_*` + émetteur + cas dans `apply_event` (+ `host_handle`), et **`NET_VER`++**.
- Toute API Windows a son équivalent `mac_*` (et Linux).
- Pas d'accents dans les chaînes C ; commentaires en français.
- Toute nouvelle mécanique a sa ligne dans `run_tests()` ; toute nouvelle scène, sa capture `--shot`.
- Tout est déterministe (graine) : génération, directeur, monstres → bugs reproductibles.
