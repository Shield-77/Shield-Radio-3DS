# SHIELD Radio

**Codex was used to generate the code**

SHIELD Radio est un lecteur de radios Internet pour 3DS, conçu pour rester utilisable sur Old 3DS. Il a besoin du Wi-Fi ; la console ne reçoit pas la FM directement.

## Version actuelle

La dernière version est **[v2.0.11](https://github.com/Shield-77/Shield-Radio-3DS/releases/tag/v2.0.11)**. Télécharger le fichier `shield-radio.cia` de cette release. Le Title ID reste `000400000F643100` pour mettre à jour une installation existante. Une ancienne version 3DSX est disponible dans la release v2.0.2 ; elle n'a pas les corrections récentes du CIA.

La v2.0.11 corrige le Bass Boost (Off, +6, +9, +16 et +20 dB), met à jour le flux DLF Kultur, affiche les codes CURL/HTTP lors des reconnexions et explique la mise en tampon dans le guide. **Shield-77 a validé cette version sur Old 3DS.** Les adresses des radios peuvent néanmoins changer indépendamment de l'application.

## Utilisation

- Activer le Wi-Fi avant de lancer l'application.
- **Haut/Bas** ou l'écran tactile : choisir une station.
- **A** : lancer ou arrêter la station choisie. **B** : arrêter la lecture.
- **L/R** : changer de station. **X** : verrouiller ou déverrouiller L/R.
- **Y** : parcourir les profils de basses. **START** : quitter.

La lecture avec le capot fermé maintient la connexion active et consomme de la batterie. Les stations et leurs flux appartiennent à leurs exploitants.

## Code source et compilation

Les dossiers [`source/`](source/) et [`include/`](include/) contiennent le code de la v2.0.11. Pour compiler, installer devkitPro avec devkitARM, libctru, citro2d, libcurl, mbedTLS et zlib, puis lancer `make`. Le CIA demande aussi les outils `makerom` et `bannertool` ; ils ne sont pas inclus ici.

Le code propre à SHIELD Radio est sous [licence MIT](LICENSE). Le décodeur minimp3 est sous [CC0](MINIMP3-LICENSE). Les marques, logos et contenus des stations ne sont pas accordés par la licence du code. Toute version modifiée doit être présentée comme une modification, sans se faire passer pour une version officielle de Shield-77. Le code a été généré avec l'aide de Codex ; la conception et les essais sur console viennent de Shield-77.
