# BOURDONNEMENT

Jeu d'horreur coopératif à 2 ou 3 joueurs dans des couloirs jaunes sans fin. En C pur, sans dépendance,
pour Windows, macOS et Linux. Même ossature technique que VELOCITY.

> Le 14 octobre 2003, Noé est entré seul dans le centre commercial abandonné des Quatre Vents.
> Il n'est jamais ressorti. Trois semaines plus tard, ses amis y entrent à leur tour.

- Plan : [`docs/PLAN.md`](docs/PLAN.md) — Scénario : [`docs/SCENARIO.md`](docs/SCENARIO.md)

## Jouer (sans rien compiler)

- **Windows** : [bourdonnement.exe](https://github.com/Archeos932/zhorror/raw/release/bourdonnement.exe) — si Windows
  avertit : *Informations complémentaires* > *Exécuter quand même*.
- **macOS** : [Bourdonnement-macOS.zip](https://github.com/Archeos932/zhorror/raw/release/Bourdonnement-macOS.zip)
  (premier lancement : clic droit > Ouvrir ; autoriser le micro si tu veux qu'il t'entende).
- **Linux** : [Bourdonnement-Linux.tar.gz](https://github.com/Archeos932/zhorror/raw/release/Bourdonnement-Linux.tar.gz).

Les binaires sont dans la branche [`release`](https://github.com/Archeos932/zhorror/tree/release), reconstruits à
chaque push sur `main`. Ensuite, le bouton **MISE A JOUR** du menu télécharge et installe la nouvelle version tout seul.

## Compiler

| Système | Commande | Résultat |
|---|---|---|
| macOS | `./build_mac.sh` | `Bourdonnement.app` |
| Linux | `./build_linux.sh` (paquets : `libx11-dev libgl-dev libasound2-dev libfreetype-dev`) | `Bourdonnement-Linux/bourdonnement` |
| Windows | `build.bat` (Visual Studio) | `bourdonnement.exe` |

## Touches

ZQSD / WASD se déplacer · Maj courir · Ctrl ou C s'accroupir · F lampe · E ramasser · A/E se pencher ·
Espace crier · Échap pause · F11 plein écran. Avec le **micro** (PARAMÈTRES) : parle fort et il vient ; chuchote.
Manette PS4 / Xbox reconnue.

## Tests

`bourdonnement --test test.txt` (tests automatiques) · `bourdonnement --shot menu.bmp` (capture).
