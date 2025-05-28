
# Exploration technique : Transmission de flux vidéo en UDP avec Video4Linux2 et réception sur un téléphone Android

## Auteurs

- Adèle ROY & Amaury GOUESBIER

## Collaborations

- Adrian BERTHELIN (Kereval)

## Objectif de l’exploration

L’objectif de cette exploration est de **mettre en place un système de transmission en temps réel d’un flux vidéo filmé par la caméra d’une Raspberry Pi 5 vers un téléphone Android**. Cette transmission devait se faire via **UDP**, un protocole léger à faible latence, souvent utilisé pour le streaming. Les étapes clés du projet comprenaient :

- La capture de la vidéo avec **Video4Linux2 (V4L2)**.
- L’encodage en **H.264**, un format largement compatible et efficace.
- L’envoi en temps réel via **UDP**.
- La réception et lecture du flux côté **Android**.

---

## Qu’est-ce que Video4Linux2 (V4L2) ?

**Video4Linux2** est une API standard du noyau Linux permettant la capture, le traitement et la sortie de flux vidéo et image. Elle est conçue pour interagir avec les périphériques vidéo (caméras, tuners TV, etc.).

Elle permet notamment de :
- Accéder à des périphériques `/dev/video*`.
- Configurer les formats de capture (résolution, compression, fréquence).
- Lire des trames brutes ou compressées.
- Contrôler des paramètres matériels (focus, balance des blancs, etc.).

### Exemple simple d'utilisation de V4L2 en C

```c
int fd = open("/dev/video0", O_RDWR);
struct v4l2_format fmt;
fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
fmt.fmt.pix.width = 640;
fmt.fmt.pix.height = 480;
fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
ioctl(fd, VIDIOC_S_FMT, &fmt);
```

---

## État de l’art

| Composant               | Solutions possibles                          | Références |
|-------------------------|-----------------------------------------------|------------|
| Capture vidéo           | Video4Linux2 (V4L2)                           | [V4L2 API](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html) |
| Encodage vidéo          | ffmpeg, x264, libavcodec                      | [H.264](https://www.dacast.com/fr/le-blog-des-experts-video/h-264-codage-video-avance/) |
| Transmission réseau     | UDP (via socket ou ffmpeg)                   | [RFC 768](https://datatracker.ietf.org/doc/html/rfc768) |
| Réception Android       | UDP + `VideoView` ou `ffmpeg`, `VLC`         | [Android VideoView](https://developer.android.com/reference/android/widget/VideoView) |

### Analyse comparative

- **V4L2** est la méthode standard sous Linux pour capturer la vidéo en basse couche. Robuste, mais complexe.
- **UDP** offre un bon compromis pour la vidéo en direct (faible latence, pas de correction d’erreur).
- **H.264**  est un format vidéo qui permet une compression efficace du flux tout en conservant une bonne qualité, mais nécessite un encodeur matériel ou logiciel pour être produit..
- Sur **Android**, `VideoView` peut lire des fichiers locaux, mais pas directement un flux UDP. Nous devions donc l'enregistrer ou le convertir.

---

## Choix techniques

Suite aux échanges avec notre enseignant, nous avons opté pour les choix suivants :

| Élément                  | Choix retenu                        | Motivation |
|--------------------------|--------------------------------------|------------|
| API de capture           | Video4Linux2 (V4L2)                  | API native, efficace, adaptée à Linux |
| Format vidéo             | H.264                                | Compression efficace, compatible Android, débit réduit |
| Transmission réseau      | UDP                                  | Faible latence, pas de surcharge |
| Réception                | Android : socket UDP + enregistrement | Lecture différée via `VideoView` |
| Encodage                 | H.264 matériel (si disponible)        | Pour limiter la charge CPU |

### Pourquoi H.264 ?

- **Compression efficace** : Très bon ratio qualité/débit.
- **Compatibilité universelle** : Android, iOS, navigateurs, VLC, etc.
- **Support matériel courant** : Certains chipsets, dont ceux des Raspberry (< Pi.5), l’implémentent en hardware.
- **Faible latence** : Adapté à la diffusion en direct.

---

## Exploration du Code : Envoi de Flux Vidéo via UDP avec V4L2

Ce document présente une analyse détaillée du code C utilisé pour capturer un flux vidéo depuis une webcam (utilisant le driver V4L2 sur Linux) et l’envoyer via UDP à un client. Ce serveur tourne typiquement sur une Raspberry Pi.

---

## Fonction principale du code C

### Ouverture de la webcam

```c
fd = open("/dev/video0", O_RDWR);
```

Ouvre le périphérique vidéo en lecture/écriture. `/dev/video0` est  la webcam par défaut et celle que nous utilisons.

---

### Configuration du format vidéo

```c
fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
```


Configure un format vidéo en H.264, ce qui est plus efficace pour la transmission réseau car ce format est compressé.

---

### Allocation et mappage mémoire des buffers

```c
req.count = 4;
req.memory = V4L2_MEMORY_MMAP;
ioctl(fd, VIDIOC_REQBUFS, &req);
```

Demande au driver V4L2 d’allouer 4 buffers pour le streaming.


Ensuite, chaque buffer est mappé avec `mmap` pour un accès rapide :

```c
buffers[n_buffers].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
```

---

### Mise en file d’attente et démarrage du streaming

```c
ioctl(fd, VIDIOC_QBUF, &buf);
ioctl(fd, VIDIOC_STREAMON, &type);
```

Tous les buffers sont mis en file d’attente, puis le flux est lancé.

---

### Boucle de capture et d’envoi

```c
while (1) {
    ioctl(fd, VIDIOC_DQBUF, &buf);
    sendto(sockfd, buffers[buf.index].start, buf.bytesused, 0, (struct sockaddr *)&clientAddr, addr_size);
    ioctl(fd, VIDIOC_QBUF, &buf);
}
```

À chaque itération :

1. On récupère une frame (`DQBUF`)
2. On l’envoie via UDP avec `sendto`
3. On remet le buffer dans la file (`QBUF`)

---

###  Fin de transmission

À la fin de la boucle :

```c
sendto(sockfd, "END", 3, 0, (struct sockaddr *)&clientAddr, addr_size);
```

Permet d'indiquer au client que la vidéo est terminée.

Puis on arrête le flux vidéo et on libère les ressources :

```c
ioctl(fd, VIDIOC_STREAMOFF, &type);
munmap(...);
free(buffers);
close(fd);
```

---

### Fonction `main()`

Elle initialise le socket UDP et attend un message du client pour établir la connexion:

```c
recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&clientAddr, &addr_size);
```

Et si ce message est `START_client`, elle appelle `envoyerFluxVideoUDP()` pour lancer l’envoi du flux.

```c
if (strncmp(buffer, "START_client", 12) == 0) {
    envoyerFluxVideoUDP(sockfd, clientAddr, addr_size);
}
```

---

## ✅ Résumé

Ce serveur UDP pour Raspberry PI :

* Utilise V4L2 pour capturer un flux vidéo en H.264
* Gère des buffers mappés en mémoire pour la performance
* Transmet le flux compressé image par image via UDP
* Signale au client le début (`START_serveur`) et la fin (`END`) de la transmission

---

Pour une communication fiable, on peut envisager une migration vers TCP ou l’utilisation de protocoles spécifiques à la vidéo (RTP/RTSP).


## Problèmes rencontrés : l’encodage H.264 sur Raspberry Pi 5

Un obstacle majeur a été découvert au cours de notre expérimentation : **la Raspberry Pi 5 ne dispose pas nativement d’un encodeur H.264 via V4L2**.

En effet, notre Raspberry Pi 5 utilise un GPU VideoCore VII, mais l'accès à l'encodage matériel (H.264) est limité. En effet, V4L2 ne propose pas de périphérique `/dev/video*` correspondant à un encodeur matériel H.264 par défaut. Il faut donc passer par **GStreamer** par exemple ou via des bibliothèques tierces comme **libcamera** avec un pipeline plus complexe.

Dans le cadre de notre projet ProSE cela signifie que :
- V4L2 seul ne suffit pas pour capturer et encoder en H.264.
- Une étape d’encodage logiciel aurait été nécessaire, ce qui est trop coûteux sur le CPU de la Raspberry Pi.


---

## Limites de l’approche V4L2 seule

- Complexité de gestion des buffers.
- Nécessité d’ajouter manuellement l’encodage (hors API V4L2).
- Pas de support natif de l'encodage H.264 matériel sur Pi5 via V4L2.

---

## Solution alternative : GStreamer

Suite à nos constats et recommandations de notre professeur, nous nous sommes tournés vers **GStreamer**, qui propose :

- Des pipelines simples et lisibles.
- Un support natif pour les encodeurs matériels de la Pi (via `v4l2h264enc`, `omxh264enc`, ou `v4l2convert`).
- Des commandes comme :

```bash
gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw,width=640,height=480 ! v4l2h264enc ! rtph264pay ! udpsink host=192.168.1.10 port=5000
```

Cette solution est bien plus adaptée à notre usage pour réaliser un streaming temps réel.

---

## Intégration de GStreamer pour le streaming vidéo H.264 et la détection de QR codes

Suite aux limitations rencontrées avec Video4Linux2 et l'encodage H.264 sur Raspberry Pi 5, nous avons opté pour **GStreamer** pour capturer la vidéo, l’encoder en H.264, et transmettre le flux via **TCP** tout en analysant les images à la volée pour détecter des **QR codes**. 

### Pipeline GStreamer utilisé

Le pipeline est composé de deux branches (grâce à `tee`) :

1. **Branche de streaming** :
   - Capture via `libcamerasrc` (caméra Pi).
   - Conversion d'image avec `videoconvert`.
   - Encodage H.264 avec `x264enc` (profil baseline, latence faible).
   - Transmission via `tcpserversink` sur le port 4000.

2. **Branche d’analyse QR** :
   - Conversion et mise à l’échelle de l’image en `GRAY8` (inutile d'avoir de la couleur pour la détection de QrCodes).
   - Récupération d’image avec `appsink`.
   - Décodage de QR codes via la bibliothèque **ZBar**.

### Exemple de pipeline GStreamer

```bash
libcamerasrc ! tee name=t 
  t. ! queue ! video/x-raw, width=640, height=480, framerate=10/1, format=I420 ! videoconvert ! 
       x264enc tune=zerolatency byte-stream=true key-int-max=30 speed-preset=ultrafast bitrate=1000 ! 
       video/x-h264, stream-format=byte-stream, alignment=au, profile=baseline ! 
       tcpserversink host=0.0.0.0 port=4000 
  t. ! queue ! videoconvert ! videoscale ! 
       video/x-raw, width=640, height=480, format=GRAY8 ! appsink name=appsink sync=false
```

### Détails du code en C

- **Initialisation** : `gst_init()` démarre GStreamer.
- **Construction du pipeline** : `gst_parse_launch()` crée dynamiquement le pipeline décrit ci-dessus.
- **Callback `on_new_sample()`** :
  - Récupère les images via `appsink`.
  - Convertit le buffer en tableau de `uint8_t`.
  - Passe l’image en niveau de gris à la fonction de détection QR.

```c
GstSample* sample = gst_app_sink_pull_sample(appsink);
GstBuffer* buffer = gst_sample_get_buffer(sample);
gst_buffer_map(buffer, &map, GST_MAP_READ);
memcpy(gray_data, map.data, width * height);
decode_qr_from_buffer(gray_data, width, height);
```

- **Fin de flux** : GStreamer gère proprement la fermeture et la désallocation de ses ressources.

### Avantages de GStreamer dans ce contexte

| Avantage | Détail |
|----------|--------|
| Modulaire | Intégration facile d’un pipeline personnalisé avec plusieurs branches |
| Support H.264 | Accès à `x264enc` même si l’encodeur matériel n’est pas exposé via V4L2 |
| Faible latence | Grâce au profil `zerolatency` et au `tcpserversink` |
| Intégration avec C | Excellente API native pour des projets embarqués en C |
---

## Réception vidéo sur Android – Analyse technique (avant utilisation de GStreamer)

Dans notre projet, la réception du flux vidéo sur Android est assurée par une application Java native. Celle-ci établit une communication **UDP client-serveur**, réceptionne les paquets vidéo envoyés, les sauvegarde dans un fichier `.mp4`, puis les lit via un composant `VideoView`.

#### Initialisation et interface

L’interface utilisateur est initialisée dans la méthode `onCreate()`, avec notamment la vérification des permissions de stockage :

```java
startButton = findViewById(R.id.startButton);
progressBar = findViewById(R.id.progressBar);
statusText = findViewById(R.id.statusText);
videoView = findViewById(R.id.videoView);

checkPermission(); // Vérifie l'accès en écriture
```

#### Connexion UDP et synchronisation

Un thread secondaire est lancé à l'appui sur un bouton pour ne pas bloquer l’interface principale (`UI thread`). La connexion se fait via UDP en envoyant un message de démarrage :

```java
InetAddress serverAddress = InetAddress.getByName("192.168.1.131");
byte[] startMsg = "START_client".getBytes();
DatagramPacket startPacket = new DatagramPacket(startMsg, startMsg.length, serverAddress, PORT_UDP);
socket.send(startPacket);
```

Le client attend ensuite une réponse `"START_serveur"` pour synchroniser le début de la transmission :

```java
DatagramPacket packet = new DatagramPacket(receiveBuffer, receiveBuffer.length);
socket.receive(packet);
String message = new String(packet.getData(), 0, packet.getLength());

if (!message.equals("START_serveur")) {
    fileOutputStream.write(packet.getData(), 0, packet.getLength());
}
```

#### Réception des paquets et enregistrement

Les paquets sont reçus un à un, écrits directement dans un fichier, et le processus s'arrête à la réception du message `"END"` :

```java
while (receiving) {
    socket.receive(packet);
    String checkMsg = new String(packet.getData(), 0, packet.getLength());

    if (packet.getLength() == 3 && checkMsg.equals("END")) {
        receiving = false;
        continue;
    }

    fileOutputStream.write(packet.getData(), 0, packet.getLength());
}
```

Une mise à jour de l’état d’avancement est affichée en temps réel :

```java
runOnUiThread(() -> statusText.setText("Received: " + totalReceived + " bytes"));
```

#### Lecture du fichier vidéo

Une fois la réception terminée, le fichier est lu avec un `VideoView` via un `FileProvider` :

```java
Uri videoUri = FileProvider.getUriForFile(this, "com.example.video4l2_3.fileprovider", videoFile);
videoView.setVideoURI(videoUri);
videoView.start();
```

---

### Limites techniques identifiées

Notre implémentation actuelle nous a permis de mieux comprendre les limites du protocole UDP et de la lecture de fichiers vidéo sur Android. Voici les principales contraintes rencontrées :

- **Bufferisation complète** :  
  La vidéo n’est lisible **qu’à la fin de la réception**. Pour une lecture **en continu**, il aurait fallu utiliser une solution comme **ExoPlayer** avec un `custom DataSource`, ou une intégration bas-niveau avec **ffmpeg + SurfaceView**.

- **Pas de correction de pertes UDP** :  
  UDP ne garantit pas la livraison des paquets. Les pertes peuvent donc rendre le fichier **partiellement illisible** ou provoquer des erreurs de décodage.

- **Pas de gestion de l’ordre des paquets** :  
  Les paquets sont écrits directement dans le fichier sans tri ni vérification de leur ordre d’arrivée, ce qui est risqué avec UDP.

- **Pas de chiffrement ni d’authentification** :  
  Toute la transmission se fait en clair, sans contrôle d’intégrité ni sécurité. Ce point serait à revoir dans un déploiement en environnement sensible. Cependant, dans le cadre de notre projet, le chiffrement TLS de la vidéo n'est pas nécessaire pour le client.

---

### Conclusion sur l’implémentation Android

Bien que notre solution de réception Android via `VideoView` et UDP soit fonctionnelle dans des conditions idéales, elle présente des **limitations fondamentales**. L’une des plus critiques est l’impossibilité de lire la vidéo en continu pendant la réception, ce qui va à l’encontre des principes d’un streaming fluide.

En explorant cette voie, nous avons appris de nombreuses choses sur :
- La gestion réseau UDP côté Android,
- La manière dont `VideoView` lit des fichiers,
- Les contraintes de lecture en streaming.

Ce travail exploratoire nous a permis de mieux **échanger avec les développeurs Android** de notre équipe, en comprenant leurs contraintes et en proposant des pistes d’amélioration réalistes (ExoPlayer, ffmpeg, etc.).

## Conclusion

Notre exploration de **Video4Linux2** nous a permis de comprendre les mécanismes bas-niveau de la capture vidéo sur Linux. Cependant, son utilisation en tant qu’outil principal pour le **streaming en H.264** sur Raspberry Pi 5 a montré ses limites, notamment l'absence d’un encodeur matériel H.264 accessible via cette API. Notre exploration de **Video4Linux2** a donc pris fin à ce moment, il aurait été possible de faire le streaming en H.264 à la main mais cela aurait été trop long, trop complexe et non adapté au cadre pédagogique du Projet ProSE.

Grâce à cette étude, nous avons conclu qu’une approche avec **GStreamer** est bien plus appropriée pour un projet embarqué avec contraintes de performances. Elle permet de simplifier considérablement la chaîne de traitement tout en tirant parti du matériel disponible. Nous avons donc pu rejoindre Antoine MARTIN et Lucas BILLIEN sur leur exploration de GStreamer afin d'implémenter la vidéo pour notre projet de Prototype de Drone de Soutien. 

---

## Sources

- [V4L2 Documentation](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html)
- [H.264 Documentation](https://www.dacast.com/fr/le-blog-des-experts-video/h-264-codage-video-avance/)
- [GStreamer Documentation](https://gstreamer.freedesktop.org/documentation/)
- [RFC 768 - UDP](https://datatracker.ietf.org/doc/html/rfc768)
- [VideoCore on Raspberry Pi](https://www.raspberrypi.com/documentation/computers/video.html)
- [Android Developer - VideoView](https://developer.android.com/reference/android/widget/VideoView)
- [libcamera](https://libcamera.org/)