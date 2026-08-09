# Les enfants des courants d'air - instructions

## Software

### Installation et usage

Pour modifier le code et l'uploader sur les ESP32, installe [VS Code](https://code.visualstudio.com) et l'extension [PlatformIO](https://platformio.org).

Tu peux ensuite ouvrir le dossier "enfants_courants_dair" sur VS Code et cliquer sur l'icône PlatformIO (tête d'alien) dans la barre latérale gauche. Tu vois alors trois dossiers (Default, piedsouffle et toit). Pour changer le code de l'ESP du toit par exemple, tu cliques sur le dossier "toit" puis sur "Upload and Monitor".

### Modifications de base

Avant toute chose, tu pourras créer un fichier "credentials.h" dans le dossier "src" pour y mettre les identifiants WiFi correspndants à ton routeur. Le fichier doit contenir les lignes suivantes :

```cpp
#define WIFI_NAME "WIFI NAME"
#define PASSWORD "password"
```


## Hardware

### Pied souffle

Voici comment brancher les éléments du pied souffle.

<img width="1512" alt="pied_souffle_schema" src="https://raw.githubusercontent.com/leohuet/enfants_courants_dair/master/include/pics/explication_1.jpg">

Lorsque la batterie est branchée et que tu le branches aussi en USB, la LED rouge intégrée se met à clignoter et la LED orange s'éteint. Cela signifie que l'ESP se met en mode "light sleep" et que la batterie se recharge. Plusieurs niveaux de charge sont affichés par la LED externe : rouge = -50% de batterie, vert = entre 50 et 80% de batterie, bleu = plus de 80% de batterie. 

