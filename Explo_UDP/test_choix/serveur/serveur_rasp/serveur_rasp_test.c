#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <errno.h>
#include <sys/mman.h>

#define PORT_UDP 12345
#define BUFFER_SIZE 1024

// Test de serveur tournant sur une Raspberry

// Structure pour stocker les informations de buffer
struct buffer {
    void *start;
    size_t length;
};

static struct buffer *buffers = NULL;
static unsigned int n_buffers = 0;

void envoyerFluxVideoUDP(int sockfd, struct sockaddr_in clientAddr, socklen_t addr_size) {
    int fd;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;
    enum v4l2_buf_type type;
    ssize_t sent;
    
    // Envoyer un signal de démarrage au client
    sendto(sockfd, "START_serveur", strlen("START_serveur"), 0, 
           (struct sockaddr *)&clientAddr, addr_size);
    
    printf("Message 'START_serveur' envoyé.\n");
    printf("Serveur UDP prêt pour la vidéo...\n");

    // Ouverture de la webcam
    fd = open("/dev/video0", O_RDWR);
    if (fd == -1) {
        perror("Erreur d'ouverture de la webcam");
        return;
    }

    // Configuration du format de la vidéo en H.264
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264; // Capture en H.264
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("Erreur de format de capture");
        close(fd);
        return;
    }

    // Vérification que la caméra a bien accepté H.264
    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_H264) {
        printf("Format H.264 non supporté par la caméra !\n");
        close(fd);
        return;
    }

    // Demande de buffers pour la capture vidéo
    memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        perror("Erreur de demande de buffers");
        close(fd);
        return;
    }

    // Allocation de mémoire pour les buffers
    buffers = calloc(req.count, sizeof(*buffers));
    if (!buffers) {
        perror("Erreur d'allocation de mémoire pour les buffers");
        close(fd);
        return;
    }

    // Mappage des buffers en mémoire
    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            perror("Erreur de requête de buffer");
            goto cleanup;
        }

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start = mmap(NULL, buf.length,
                                        PROT_READ | PROT_WRITE, MAP_SHARED,
                                        fd, buf.m.offset);

        if (buffers[n_buffers].start == MAP_FAILED) {
            perror("Erreur de mappage mémoire");
            goto cleanup;
        }
        
        // Mise en file d'attente du buffer
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("Erreur de mise en file d'attente du buffer");
            goto cleanup;
        }
    }

    // Démarrage du streaming
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        perror("Erreur de démarrage du stream");
        goto cleanup;
    }

    // Capture et envoi des images H.264
    while (1) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        
        if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
            perror("Erreur de récupération de buffer");
            break;
        }

        // Envoi des données H.264 via UDP
        sent = sendto(sockfd, buffers[buf.index].start, buf.bytesused, 0, 
                      (struct sockaddr *)&clientAddr, addr_size);
        
        if (sent < 0) {
            perror("Erreur envoi frame");
            break;
        }

        printf("Frame envoyée, taille: %d bytes\n", buf.bytesused);

        // Remise du buffer dans la file d'attente
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("Erreur de remise en file d'attente du buffer");
            break;
        }
    }

    // Envoi du message de fin
    sendto(sockfd, "END", 3, 0, (struct sockaddr *)&clientAddr, addr_size);
    printf("Vidéo envoyée et signal de fin envoyé !\n");

    // Arrêt du streaming
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
        perror("Erreur d'arrêt du stream");
    }

cleanup:
    // Libération des ressources
    for (unsigned int i = 0; i < n_buffers; ++i) {
        if (buffers[i].start != MAP_FAILED && buffers[i].start != NULL) {
            munmap(buffers[i].start, buffers[i].length);
        }
    }
    free(buffers);
    close(fd);
}

int main() {
    int sockfd;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE];
    
    // Création du socket UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Erreur de socket UDP");
        exit(EXIT_FAILURE);
    }
    
    // Configuration de l'adresse du serveur
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT_UDP);
    serverAddr.sin_addr.s_addr = INADDR_ANY; // Utiliser INADDR_ANY au lieu d'une IP spécifique
    
    // Bind du socket
    if (bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Erreur de bind UDP");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    printf("Serveur UDP en attente de connexion sur le port %d...\n", PORT_UDP);
    
    // Attente de connexion client
    addr_size = sizeof(clientAddr);
    recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&clientAddr, &addr_size);
    printf("Message reçu du client: %s\n", buffer);
    
    // Si le message est START_client, commencer l'envoi vidéo
    if (strncmp(buffer, "START_client", 12) == 0) {
        printf("Client prêt pour la vidéo...\n");
        envoyerFluxVideoUDP(sockfd, clientAddr, addr_size);
    } else {
        printf("Message incorrect reçu: %s\n", buffer);
    }
    
    close(sockfd);
    return 0;
}