#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>

// ====== Video4Linux ======
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <errno.h>
#include <sys/mman.h>  // Ajout de l'en-tête pour mmap
// =========================

// Test d'un serveur pouvanr gérer l'UDP et le TCP au choix en utilisant Video4Linux

#define PORT_TCP 8080
#define PORT_UDP 12345
#define BUFFER_SIZE 1024
#define VIDEO_FILE "IMG_5362.mp4"

void communicationUDP();
void communicationTCP();
void envoyerVideoUDP(FILE* video);
void envoyerFluxVideoUDP();

// Déclaration des variables globales
struct buffer *buffers;
unsigned int n_buffers = 0;
static int sockfd;
static int server_fd;
static char buffer[BUFFER_SIZE];
static struct sockaddr_in clientAddr;
static socklen_t addr_size;
volatile int serveurActif = 1;  // Indique si le serveur doit continuer à tourner

// Structure pour stocker les buffers mmap
struct buffer {
    void* start;
    size_t length;
};

// Tableau de pointeurs de fonctions
void (*choixCommunicationServeur[])() = {communicationUDP, communicationTCP};

/*------------------------------------------------------------------------------------------*/

void communicationUDP() {
    FILE* video;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Erreur de socket UDP");
        exit(EXIT_FAILURE);
    }
    printf("1\n");

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT_UDP);
    //serverAddr.sin_addr.s_addr = inet_addr("172.23.6.24");
    serverAddr.sin_addr.s_addr = inet_addr("192.168.1.1");
    
    printf("2\n");

    if (bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Erreur de bind UDP");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Serveur UDP en attente de connexion...\n");
    addr_size = sizeof(clientAddr);
    recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&clientAddr, &addr_size);
    printf("Client prêt pour la vidéo...\n");

    //envoyerVideoUDP(video);
    envoyerFluxVideoUDP();
    //close(sockfd);
}

/*------------------------------------------------------------------------------------------*/

void communicationTCP() {
    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("Erreur de socket TCP");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    //address.sin_addr.s_addr = inet_addr("172.23.6.24");
    address.sin_addr.s_addr = inet_addr("192.168.1.1");
    address.sin_port = htons(PORT_TCP);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Erreur de bind TCP");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Erreur d'écoute TCP");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Serveur TCP en attente de connexion...\n");
    new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
    if (new_socket < 0) {
        perror("Erreur d'acceptation TCP");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    read(new_socket, buffer, BUFFER_SIZE);
    printf("Message reçu en TCP: %s\n", buffer);

    close(new_socket);
    //close(server_fd);
}

/*------------------------------------------------------------------------------------------*/

void envoyerVideoUDP(FILE* video) {
    video = fopen(VIDEO_FILE, "rb");
    if (!video) {
        perror("Erreur ouverture fichier vidéo");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Send START message 
    ssize_t sent = sendto(sockfd, "START_serveur", strlen("START_serveur"), 0, 
                         (struct sockaddr *)&clientAddr, addr_size);
    if (sent < 0) {
        perror("Erreur envoi START_serveur");
        fclose(video);
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("Message 'START_serveur' envoyé.\n");

    printf("Lecture de la vidéo et envoi des bytes\n");
    
    // Add small delay to ensure client is ready
    usleep(500000);  // 500ms delay
    
    // Send video packets
    size_t totalSent = 0;
    while (!feof(video)) {
        size_t bytesRead = fread(buffer, 1, BUFFER_SIZE, video);
        if (bytesRead <= 0) break;
        
        ssize_t sent = sendto(sockfd, buffer, bytesRead, 0, 
                            (struct sockaddr *)&clientAddr, addr_size);
        if (sent < 0) {
            perror("Erreur envoi vidéo");
            break;
        }
        totalSent += sent;
        usleep(10000);  // 10ms pause between packets
    }
    printf("Total bytes envoyés: %zu\n", totalSent);

    // Send END signal
    sendto(sockfd, "END", 3, 0, (struct sockaddr *)&clientAddr, addr_size);
    printf("Vidéo envoyée et signal de fin envoyé !\n");

    fclose(video);
}

/*------------------------------------------------------------------------------------------*/

// Fonction pour lancer FFmpeg en utilisant la commande donnée
void lancerFFmpeg() {
    char command[256];
    
    // Crée la commande pour lancer FFmpeg
    snprintf(command, sizeof(command),
             "ffmpeg -f rawvideo -pix_fmt yuyv422 -s 640x480 -i - -c:v libx264 -f mpegts udp://192.168.1.1:12345");
    
    // Exécute la commande FFmpeg
    if (system(command) == -1) {
        perror("Erreur lors du lancement de FFmpeg");
    }
}

/*------------------------------------------------------------------------------------------*/

// Fonction pour envoyer un flux vidéo via UDP en utilisant la webcam et l'encodage H.264
void envoyerFluxVideoUDP() {
    int fd;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;
    enum v4l2_buf_type type;
    FILE *ffmpeg_pipe = NULL;

    // Journal de débogage complet
    FILE *debug_log = fopen("/tmp/video_streaming_debug.log", "w");
    if (!debug_log) {
        perror("ERREUR: Impossible de créer le fichier de débogage");
        return;
    }

    // Horodatage pour le débogage
    time_t now;
    struct tm *t;
    time(&now);
    t = localtime(&now);
    fprintf(debug_log, "Début de la capture vidéo: %s\n", asctime(t));

    // Ouvrir la webcam avec des informations détaillées
    fd = open("/dev/video0", O_RDWR);
    if (fd == -1) {
        fprintf(debug_log, "ERREUR CRITIQUE: Impossible d'ouvrir /dev/video0\n");
        fprintf(debug_log, "Détails errno: %d (%s)\n", errno, strerror(errno));
        
        // Lister les périphériques vidéo disponibles
        system("ls /dev/video* >> /tmp/video_streaming_debug.log 2>&1");
        
        fclose(debug_log);
        return;
    }
    fprintf(debug_log, "✓ Webcam /dev/video0 ouverte avec succès\n");

    // Vérification détaillée des capacités
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        fprintf(debug_log, "ERREUR: Impossible de vérifier les capacités\n");
        fprintf(debug_log, "Détails errno: %d (%s)\n", errno, strerror(errno));
        
        close(fd);
        fclose(debug_log);
        return;
    }

    // Configuration du format vidéo
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        fprintf(debug_log, "ERREUR: Configuration du format impossible\n");
        fprintf(debug_log, "Détails errno: %d (%s)\n", errno, strerror(errno));
        
        close(fd);
        fclose(debug_log);
        return;
    }

    // Demande de buffers
    memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        fprintf(debug_log, "ERREUR: Impossible de demander les buffers\n");
        fprintf(debug_log, "Détails errno: %d (%s)\n", errno, strerror(errno));
        
        close(fd);
        fclose(debug_log);
        return;
    }

    // Allocation et mappage des buffers
    buffers = calloc(req.count, sizeof(*buffers));
    if (!buffers) {
        fprintf(debug_log, "ERREUR: Allocation mémoire impossible\n");
        close(fd);
        fclose(debug_log);
        return;
    }

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            fprintf(debug_log, "ERREUR: Impossible de requêter le buffer %d\n", n_buffers);
            fprintf(debug_log, "Détails errno: %d (%s)\n", errno, strerror(errno));
            goto cleanup;
        }

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start = mmap(NULL, buf.length, 
                                        PROT_READ | PROT_WRITE, 
                                        MAP_SHARED, fd, buf.m.offset);

        if (buffers[n_buffers].start == MAP_FAILED) {
            fprintf(debug_log, "ERREUR: Mappage mémoire impossible pour le buffer %d\n", n_buffers);
            fprintf(debug_log, "Détails errno: %d (%s)\n", errno, strerror(errno));
            goto cleanup;
        }

        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            fprintf(debug_log, "ERREUR: Impossible de mettre le buffer %d en file d'attente\n", n_buffers);
            fprintf(debug_log, "Détails errno: %d (%s)\n", errno, strerror(errno));
            goto cleanup;
        }
    }

    // Démarrage du streaming
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        fprintf(debug_log, "ERREUR: Impossible de démarrer le streaming\n");
        fprintf(debug_log, "Détails errno: %d (%s)\n", errno, strerror(errno));
        goto cleanup;
    }

    // Préparation du pipe FFmpeg
    ffmpeg_pipe = popen(
        "ffmpeg -f rawvideo -pix_fmt yuyv422 -s 640x480 -i - "
        "-c:v libx264 -f mpegts udp://192.168.1.1:12345", "w"
    );
    
    if (!ffmpeg_pipe) {
        fprintf(debug_log, "ERREUR: Impossible de créer le pipe FFmpeg\n");
        goto cleanup;
    }

    // Capture et envoi des frames
    for (int i = 0; i < 100; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
            fprintf(debug_log, "ERREUR: Impossible de déqueue le buffer\n");
            fprintf(debug_log, "Frame %d, Détails errno: %d (%s)\n", i, errno, strerror(errno));
            break;
        }

        fwrite(buffers[buf.index].start, 1, buf.bytesused, ffmpeg_pipe);
        fprintf(debug_log, "Frame %d envoyée, taille: %d bytes\n", i + 1, buf.bytesused);

        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            fprintf(debug_log, "ERREUR: Impossible de requeue le buffer\n");
            break;
        }
    }

cleanup:
    // Libération des ressources
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMOFF, &type);

    for (unsigned int i = 0; i < n_buffers; ++i) {
        if (buffers[i].start != MAP_FAILED && buffers[i].start != NULL) {
            munmap(buffers[i].start, buffers[i].length);
        }
    }
    free(buffers);

    if (ffmpeg_pipe) pclose(ffmpeg_pipe);
    close(fd);

    // Fin du log
    time(&now);
    t = localtime(&now);
    fprintf(debug_log, "Fin de la capture vidéo: %s\n", asctime(t));
    fclose(debug_log);
}

/*------------------------------------------------------------------------------------------*/

void fermerServeur(int sig) {
    printf("\nInterruption détectée, arrêt du serveur...\n");
    serveurActif = 0;

    if (sockfd != -1) {
        close(sockfd);
        printf("Socket UDP fermée.\n");
    }
    if (server_fd != -1) {
        close(server_fd);
        printf("Socket TCP fermée.\n");
    }
    
    exit(0);
}


/*------------------------------------------------------------------------------------------*/

// Fonction principale qui permet à l'utilisateur de choisir entre UDP et TCP
int main() {
    int choix;
    signal(SIGINT, fermerServeur);
    // Demande à l'utilisateur de choisir entre UDP ou TCP pour la communication
    printf("Serveur - Choisissez le mode de communication :\n1. UDP\n2. TCP\n");
    scanf("%d", &choix);  // Lecture du choix de communication

    // Exécution de la fonction correspondant au choix de l'utilisateur
    if (choix == 1 || choix == 2) {
        (*choixCommunicationServeur[choix - 1])();  // Appel de la fonction pour UDP ou TCP
    } else {
        // Message d'erreur si l'utilisateur choisit une option incorrecte
        printf("Choix invalide.\n");
    }

    // Fermeture des sockets à la fin de l'exécution
    close(sockfd);  // Fermeture de la socket UDP
    close(server_fd);  // Fermeture de la socket TCP

    return 0;  // Fin du programme
}
