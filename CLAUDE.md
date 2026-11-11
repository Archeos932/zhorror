# BOURDONNEMENT — guide pour Claude

Jeu d'horreur coopératif (2 à 3 joueurs) dans des backrooms, en **C pur**, une seule unité de compilation
(`main.c` inclut les morceaux de `src/` dans l'ordre), sans aucune dépendance : Win32/Cocoa/X11 + OpenGL 1.1
(pipeline fixe) + son synthétisé + (à venir) UDP. Même ossature que **VELOCITY** (`~/GitHub/velocity`), dont une
partie du code est reprise : avant de réécrire un système, regarder s'il existe là-bas (`velocity/docs/CODE.md`).
Le propriétaire est francophone : réponses, commentaires et docs **en français**.

- Plan, architecture cible et étapes : **`docs/PLAN.md`** (§9 = étapes ; on en est à la fin de l'**étape 0**).
- Histoire, textes à l'écran, fins : **`docs/SCENARIO.md`**.
- Nouvelle fonction : dans le fichier de son thème ; nouveau fichier : l'ajouter à `main.c` au bon endroit
  (l'ordre compte : une fonction doit être définie plus haut que celui qui l'utilise).
- Port macOS (`src/plateforme/mac/`) et Linux (`src/plateforme/linux/`) repris de VELOCITY : toute API Win32 a son
  équivalent `mac_*`, implémenté dans les deux couches.

## Compiler

- Mac : `./build_mac.sh` → `Bourdonnement.app` (binaire `Bourdonnement.app/Contents/MacOS/bourdonnement`).
- Linux : `./build_linux.sh` ; Windows : `build.bat` (MSVC via vswhere).
- **Zéro avertissement** (`-Wall`) : corriger tout avertissement. Les petites fonctions utilitaires pas encore
  utilisées sont `static inline` (pas d'avertissement « unused »).

## Vérifier (toujours, après chaque changement)

1. **Tests** : `bourdonnement --test test.txt` (sans fenêtre) → chaque ligne donne la valeur et « (attendu …) » ;
   code de sortie 1 si un test échoue. Toute nouvelle mécanique reçoit sa ligne dans `run_tests()`
   (`src/tests/tests.c`) ; les tests pilotent le joueur par `botHeld[A_*]` + `game_tick(TICK)`.
2. **Captures** : `bourdonnement --shot X.bmp` ; le nom choisit la scène (`menu`, sinon en jeu). Sur Mac :
   `sips -s format png X.bmp --out X.png` pour la regarder.
3. Supprimer ensuite les fichiers temporaires (`test.txt`, `*.bmp`, `*.png`).

## Règles du code (héritées de VELOCITY)

- **Réglages** : tous les `#define` dans `src/reglages/reglages.c`.
- **Entrées** : la logique de jeu ne lit **jamais** `keys[]` : seulement les actions `act[A_*]` (tenu) et
  `actP[A_*]` (appui du tick), remplies par `input_poll()`. Nouvelle action = `A_*` + `actId` + `actLabel` +
  `bindDefault` + `padBindDefault` (`src/entrees/actions.c`).
- **Tick fixe 120 Hz** (`TICK`) : toute la simulation dans `game_tick` ; `render(alpha)` interpole.
- **Déterminisme** : tout ce qui est aléatoire passe par `rng` / `frand()` (graine partagée en réseau plus tard) ;
  `make_sounds` sauvegarde et restaure `rng`.
- **Son** : tout est synthétisé dans `src/son/sons.c` (`S_*`), aucun fichier son. Une boucle (`play_loop`) doit
  durer un nombre entier de périodes pour se raccorder sans clic (test « bourdon se raccorde »).
- **Texte à l'écran** : police CP1252 et source en UTF-8 → **pas d'accents dans les chaînes C** (écrire `\xE9`
  si besoin). Commentaires du code en français sans accents.
- **Réseau** (étape 4) : reprendre `src/reseau/` de VELOCITY ; l'hôte (id 0) fait autorité sur monstres, objets,
  portes, morts et directeur ; incrémenter `NET_VER` à chaque changement de paquet ou d'événement.

## État actuel (jouable en solo)

- **Monde en grille** (`src/monde/carte.c`) : cases de 1 m, néons, ascenseur, fusibles, sol (`SOL_EAU`, `SOL_VERRE`),
  pièges (`PIEGE_MACHOIRE`, `PIEGE_ALARME`), `lumCase`. Niveau 1 fait à la main (`niveau_essai.c`, légende dans
  `carte_charger`), niveaux 2+ générés (`generation.c`, réglés par `genEau`/`genVerre`/`genMachoires`/`genAlarmes`).
- **Lumière** (`src/rendu/eclairage.c`, `monde.c`) : lightmaps précalculées (fixe + néons dynamiques), lampe projetée.
- **Monstres** (`src/monstres/monstres.c`, tableau `mon[]`) : Rôdeur (bruit), Locataire (n'avance que dans le noir),
  Rampante (fonce sur la lumière). Composition par niveau : `niveau_monstres` (`src/jeu/niveaux.c`).
- **Peur** (`src/jeu/screamers.c`) : BPM du joueur, screamer par monstre, apparitions, subliminales (option `flashsOn`).
- **Musique** (`src/son/ambiance.c`, inclus par `audio.c`) : synthèse temps réel pilotée par `ambTension` et
  `amb_battement` (le cœur). Son propre hasard (`ambRng`) : ne touche pas à `rng`.
- **Histoire** : `src/jeu/histoire.c` (cartons, radio) ; textes dans `outils/textes.py` → **regénérer**
  `src/jeu/textes.c` avec `python3 outils/textes.py` (accents convertis en CP1252). Ne pas modifier `textes.c` à la main.
- **Micro** (`src/son/micro.c` ; plateforme : `mac_mic_*` dans `mac_platform.m` et `linux_platform.c`, waveIn sous
  Windows) : `micro_bruit()` s'ajoute à `P.bruit`. macOS : `NSMicrophoneUsageDescription` dans l'Info.plist (build_mac.sh).
- **Réglages** : `bourdonnement.ini` (`src/interface/config.c`), écran PARAMÈTRES (`menus.c`).
- **Mise à jour** (`src/interface/mise_a_jour.c`, repris de VELOCITY) : lit `version.txt` de la branche `release`
  (remplie par `.github/workflows/release.yml` à chaque push sur `main`, build = `BOURDON_BUILD` = run_number).
  **Pousser sur `main` = publier chez tous les joueurs.**
- Captures `--shot` : `menu`, `param`, `jeu`, `noir`, `flaque`, `piege`, `rodeur`, `locataire`, `rampante`,
  `screamer`, `mort`, `carton`, `ascenseur`, `piliers`, `niveau3`.
- **Pas encore** : réseau (étape 4).
