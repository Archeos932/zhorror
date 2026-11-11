# BOURDONNEMENT

Jeu d'horreur coopératif à 2 ou 3 joueurs dans des couloirs jaunes sans fin. En C pur, sans dépendance,
pour Windows, macOS et Linux. Même ossature technique que VELOCITY.

> Le 14 octobre 2003, Noé est entré seul dans le centre commercial abandonné des Quatre Vents.
> Il n'est jamais ressorti. Trois semaines plus tard, ses amis y entrent à leur tour.

- Plan : [`docs/PLAN.md`](docs/PLAN.md) — Scénario : [`docs/SCENARIO.md`](docs/SCENARIO.md)

## Compiler

| Système | Commande | Résultat |
|---|---|---|
| macOS | `./build_mac.sh` | `Bourdonnement.app` |
| Linux | `./build_linux.sh` (paquets : `libx11-dev libgl-dev libasound2-dev libfreetype-dev`) | `Bourdonnement-Linux/bourdonnement` |
| Windows | `build.bat` (Visual Studio) | `bourdonnement.exe` |

## Touches

ZQSD / WASD se déplacer · Maj courir · Ctrl ou C s'accroupir · Échap pause · F11 plein écran.
Manette PS4 / Xbox reconnue.

## Tests

`bourdonnement --test test.txt` (tests automatiques) · `bourdonnement --shot menu.bmp` (capture).
