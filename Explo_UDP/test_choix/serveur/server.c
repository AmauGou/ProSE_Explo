// ====== SERVER.C ======

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

// ====== Video4Linux ======
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <errno.h>
#include <sys/mman.h>  // Ajout de l'en-tête pour mmap
// =========================

#define PORT_TCP 8080
#define PORT_UDP 12345
#define BUFFER_SIZE 1024
#define VIDEO_FILE "IMG_5362.mp4"

// Abandon suite aux conseils de Matthias à cause du format vidéo

void communicationUDP();
void communicationTCP();
void envoyerVideoUDP(FILE* video);
void envoyerFluxVideoUDP();

static int sockfd;
static int server_fd;
static char buffer[BUFFER_SIZE];
static struct sockaddr_in clientAddr;
static socklen_t addr_size;

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
    serverAddr.sin_addr.s_addr = inet_addr("172.23.6.24");
    
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
    address.sin_addr.s_addr = inet_addr("172.23.6.24");
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

// Fonction pour envoyer un flux vidéo via UDP en utilisant la webcam
void envoyerFluxVideoUDP() {
    int fd;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;
    void *buffer_start = NULL;  // Pointeur pour la mémoire mappée des buffers vidéo

    // Envoi du message START pour indiquer le début de la vidéo
    ssize_t sent = sendto(sockfd, "START_serveur", strlen("START_serveur"), 0, 
                         (struct sockaddr *)&clientAddr, addr_size);
    if (sent < 0) {
        perror("Erreur envoi START_serveur");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("Message 'START_serveur' envoyé.\n");

    printf("Serveur UDP prêt pour la vidéo...\n");
    
    // Ouverture de la webcam (/dev/video0)
    fd = open("/dev/video0", O_RDWR);
    if (fd == -1) {
        perror("Erreur d'ouverture de la webcam");
        exit(EXIT_FAILURE);
    }

    // Configuration du format de la vidéo (résolution, format de pixel)
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;  // Largeur de l'image
    fmt.fmt.pix.height = 480; // Hauteur de l'image
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;  // Format de pixel YUYV
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    // Envoi de la configuration à la webcam
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("Erreur de format de capture");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Demande de buffers pour la capture vidéo
    req.count = 4;  // Demande de 4 buffers pour la capture
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        perror("Erreur de demande de buffers");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Mappage des buffers en mémoire partagée
    for (int i = 0; i < req.count; i++) {
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        // Vérification des informations sur chaque buffer
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            perror("Erreur de requête de buffer");
            close(fd);
            exit(EXIT_FAILURE);
        }

        // Mappage du buffer en mémoire pour y accéder plus rapidement
        buffer_start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (buffer_start == MAP_FAILED) {
            perror("Erreur de mappage mémoire");
            close(fd);
            exit(EXIT_FAILURE);
        }

        // Mise en file d'attente du buffer pour la capture vidéo
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("Erreur de mise en file d'attente du buffer");
            close(fd);
            exit(EXIT_FAILURE);
        }
    }

    // Démarrage du flux vidéo (streaming)
    if (ioctl(fd, VIDIOC_STREAMON, &buf.type) == -1) {
        perror("Erreur de démarrage du stream");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Envoi continu des frames vidéo capturées
    size_t totalSent = 0;  // Variable pour suivre le nombre total de bytes envoyés
    while (1) {
        // Capture une image de la vidéo en mettant à jour le buffer
        if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
            perror("Erreur de capture d'image");
            break;
        }

        // Vérification de la taille du paquet et envoi du paquet vidéo
        size_t packetSize = buf.bytesused;
        if (packetSize > BUFFER_SIZE) {
            printf("La taille du paquet excède la capacité du buffer. Découpage en morceaux...\n");
            size_t bytesSent = 0;

            // Découpe et envoi du paquet en morceaux si sa taille dépasse le buffer
            while (bytesSent < packetSize) {
                size_t chunkSize = (packetSize - bytesSent) > BUFFER_SIZE ? BUFFER_SIZE : (packetSize - bytesSent);
                ssize_t sent = sendto(sockfd, buffer_start + bytesSent, chunkSize, 0, 
                                      (struct sockaddr *)&clientAddr, addr_size);
                if (sent < 0) {
                    perror("Erreur envoi vidéo");
                    break;
                }
                bytesSent += sent;
                usleep(10000);  // Petite pause entre les envois de chaque morceau (10ms)
            }
        } else {
            // Envoi d'un paquet complet si sa taille est inférieure ou égale à la capacité du buffer
            ssize_t sent = sendto(sockfd, buffer_start, packetSize, 0, 
                                  (struct sockaddr *)&clientAddr, addr_size);
            if (sent < 0) {
                perror("Erreur envoi vidéo");
                break;
            }
        }
        
        totalSent += buf.bytesused;  // Mise à jour du total des bytes envoyés

        // Mise en file d'attente du buffer après capture pour permettre la prochaine image
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("Erreur de mise en file d'attente du buffer après capture");
            break;
        }
    }

    // Affichage du nombre total de bytes envoyés
    printf("Total bytes envoyés: %zu\n", totalSent);

    // Envoi du message END pour signaler la fin de l'envoi de la vidéo
    sendto(sockfd, "END", 3, 0, (struct sockaddr *)&clientAddr, addr_size);
    printf("Vidéo envoyée et signal de fin envoyé !\n");

    // Fermeture de la connexion avec la webcam (libération des ressources)
    close(fd);  
}

/*------------------------------------------------------------------------------------------*/

// Fonction principale qui permet à l'utilisateur de choisir entre UDP et TCP
int main() {
    int choix;
    
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