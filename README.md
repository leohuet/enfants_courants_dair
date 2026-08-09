# Les enfants des courants d'air - instructions

## Software

### Installation et usage

Pour modifier le code et l'uploader sur les ESP32, installe [VS Code](https://code.visualstudio.com) et l'extension [PlatformIO](https://platformio.org).

Tu peux ensuite ouvrir le dossier "enfants_courants_dair" sur VS Code et cliquer sur l'icône PlatformIO (tête d'alien) dans la barre latérale gauche. Tu vois alors trois dossiers (Default, piedsouffle et toit). Pour changer le code de l'ESP du toit par exemple, tu branches ton ESP à l'ordinateur en USB, tu cliques sur le dossier "toit" puis sur "Upload and Monitor".
Si tu as placé ton ESP en hauteur et que ça devient compliqué d'y accéder en USB, tu peux utiliser la fonction "Over The Air" pour uploader le code par wifi/ethernet. Pour ça, dans le fichier "platformio.ini", tu peux ajouter, sous "[env:piedsouffle]" ou "[env:toit]" (en fonction de celui avec lequel tu travailles) :

```cpp
upload_protocol = espota
upload_flags =
  --auth=question
upload_port = XXX.XXX.XXX.XXX
```

avec l'adresse IP de l'ESP comme upload_port.

### Modifications de base

Avant toute chose, tu pourras créer un fichier "credentials.h" dans le dossier "src" pour y mettre les identifiants WiFi correspndants à ton routeur. Le fichier doit contenir les lignes suivantes :

```cpp
#define WIFI_NAME "WIFI NAME"
#define PASSWORD "password"
```

L'ESP va automatiquement d'abord essayer de se connecter en ethernet, et si ça ne marche pas, il va se connecter en WiFi. Au démaarrage, la LED externe va clignoter en rouge pendant qu'elle cherche à se connecter. Si la LED externe est fixe, c'est que l'ESP est connecté au réseau. 

### Modifications des paramètres

Pour modifier certains paramètres de l'ESP (adresse IP, port, etc.), j'utilise ESPUI, une interface web qui permet de modifier les paramètres en direct. Pour y accéder, il faut que l'ESP soit connecté en WiFi et que tu ailles sur l'adresse IP de l'ESP dans ton navigateur. L'addresse IP est affichée dans le moniteur série de PlatformIO quand tu vas dans "Monitor". Sinon, tu peux utiliser l'application [Angry IP Scanner](https://angryip.org) pour trouver l'adresse IP de l'ESP sur ton réseau.

<img width="1512" alt="ESPUI_schema" src="https://raw.githubusercontent.com/leohuet/enfants_courants_dair/master/include/pics/espui_1.jpg">

<img width="1512" alt="ESPUI_schema" src="https://raw.githubusercontent.com/leohuet/enfants_courants_dair/master/include/pics/espui_2.jpg">



## Hardware

### Pied souffle

Voici comment brancher les éléments du pied souffle.

<img width="1512" alt="pied_souffle_schema" src="https://raw.githubusercontent.com/leohuet/enfants_courants_dair/master/include/pics/explication_1.jpg">

Lorsque la batterie est branchée et que tu le branches aussi en USB, la LED rouge intégrée se met à clignoter et la LED orange s'éteint. Cela signifie que l'ESP se met en mode "light sleep" (l'ESP coupe le WiFi et va juste vérifier l'état de la batterie toutes les 30 min) et que la batterie se recharge. Plusieurs niveaux de charge sont affichés par la LED externe : rouge = -50% de batterie, vert = entre 50 et 80% de batterie, bleu = plus de 80% de batterie. Quand tu débranches l'USB, la LED intégrée orange se rallume, l'ESP se réveille et se reconnecte. 


### Toit

Voici comment brancher les éléments du toit.

<img width="1512" alt="toit_schema" src="https://raw.githubusercontent.com/leohuet/enfants_courants_dair/master/include/pics/explication_2.jpg">

Tu peux l'allumer en le branchant en USB. Ici, la LED externe est bleue quand le toit est connecté en WiFi ou en ethernet. 