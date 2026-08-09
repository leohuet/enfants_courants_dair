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

