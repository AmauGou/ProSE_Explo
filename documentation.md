# Documentation 

| Abbréviations | Significations |
| ------------- | -------------- |
| V4L2 | Video4Linux2 |

## Vue d'ensemble du fonctionnement

L'objectif est d'envoyer un flux vidéo capturé par une caméra via le réseau en utilisant le protocole UDP. Pour cela, on va :

1. Capturer la vidéo avec V4L2 : récupérer les images de la caméra.
2. Encoder la vidéo en H.264 (si la caméra ne le fait pas nativement).
3. Transmettre le flux via UDP : envoyer les paquets de données sur le réseau.
4. Réception et lecture : recevoir les paquets et les reconstruire pour afficher le flux.

### 1. Capture vidéo avec V4L2

V4L2 est une interface du noyau Linux permettant d’interagir avec des périphériques vidéo (comme une webcam).

On va ouvrir `/dev/videoX`, configurer le format de capture, puis récupérer les images brutes.

#### Fonctionnement de V4L2 : 

- Ouverture du périphérique vidéo (`/dev/videoX`).
- Configuration du format vidéo (résolution, format, etc.).
- Allocation des tampons mémoire (buffer) pour stocker les images.
- Démarrage du streaming (mode capture en continu).
- Lecture des images capturées (boucle de capture).
- Libération des ressources et fermeture.

### 2. Encodage en H.264

- Certaines caméras peuvent envoyer directement un flux H.264 (idéal).
- Si ce n'est pas le cas, il faudra encoder le flux en H.264 en utilisant `x264` ou `ffmpeg`.

### 3. Transmission du flux via UDP

UDP (User Datagram Protocol) est un protocole de communication léger et rapide, mais non fiable (pas de correction d’erreurs, d’accusé de réception).

Le flux H.264 est découpé en paquets puis envoyés via un socket UDP.

### 4. Réception et affichage du flux

Le client (récepteur) ouvre un socket UDP pour écouter sur un port spécifique et récupérer les paquets.

Le flux est ensuite décodé et affiché avec `ffplay`, `VLC` ou via un programme personnalisé.

## Envoyer un flux vidéo en H.264 via UDP 

### 1. Configurer la cpature vidéo avec V4L2

Vérifier que la caméra est détectée : ` v4l2-ctl --list-devices`

Lister les formats supportés : ǜ4l2-ctl --list-formats`

Sélectionner le format H.264 si la caméra le supporte : `4l2-ctl --set-fmt-video=width=1280,height=720,pixelformat=H264`

### 2. Capturer le flux vidéo 

Lire un flux brut depuis `/dev/videoX` : `cat/dev/videoX > output.h264`

ou avec `ffmpeg`: ` ffmpeg -f v4l2 -input_format h264 -i /dev/videoX -c:v copy output.h264`

### 3. Envoyer le flux en UDP

Avec `ffmpeg` : ` ffmpeg -f v4l2 -input_format h264 -i /dev/videoX -c:v copy -f mpegts udp://192.168.1.100:1234`

Remplacer 192.168.1.100 par l'IP du récpeteur et 1234 par le port correspondant

### 4. Recevoir et afficher le flux

Avec `ffplay`sur la machine réceptrice : `ffplay -fflags nobuffer -flags low_delay -i udp://@:1234`

Ou avec `vlc` : ` vlc udp://@:1234`