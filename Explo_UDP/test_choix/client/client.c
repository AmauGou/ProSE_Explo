// ====== CLIENT.C ======

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT_TCP 8080
#define PORT_UDP 12345
#define BUFFER_SIZE 1024
#define OUTPUT_VIDEO "IMG_5362.mp4"

void recevoirUDP();
void envoyerTCP();

// Tableau de pointeurs de fonctions
void (*choixCommunicationClient[])() = {recevoirUDP, envoyerTCP};

/*------------------------------------------------------------------------------------------*/

void recevoirUDP() {
    int sockfd;
    struct sockaddr_in serverAddr;
    char buffer[BUFFER_SIZE];
    FILE *video;
    socklen_t addr_size = sizeof(serverAddr);
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Erreur de socket UDP");
        exit(EXIT_FAILURE);
    }
    
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT_UDP);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    // Envoyer le message START pour démarrer la communication
    sendto(sockfd, "START_client", strlen("START_client"), 0, (struct sockaddr *)&serverAddr, addr_size);
    printf("Message 'START_client' envoyé.\n");
    
    // Attente du message START_serveur
    ssize_t bytesReceived = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, 
                                   (struct sockaddr *)&serverAddr, &addr_size);
    
    if (bytesReceived <= 0) {
        perror("Erreur de réception du message START_serveur");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    // Check if first packet is START_serveur
    buffer[bytesReceived] = '\0';
    if (strncmp(buffer, "START_serveur", 13) == 0) {
        printf("Message 'START_serveur' reçu, début de la réception de la vidéo.\n");
    } else {
        printf("Premier paquet reçu n'est pas START_serveur: %s\n", buffer);
        // Continue anyway, might be first video packet
    }
    
    video = fopen(OUTPUT_VIDEO, "wb");
    if (!video) {
        perror("Erreur ouverture fichier vidéo");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    size_t totalReceived = 0;
    int timeout_count = 0;
    
    while (1) {
        bytesReceived = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, 
                               (struct sockaddr *)&serverAddr, &addr_size);
        
        if (bytesReceived <= 0) {
            // Handle timeouts gracefully
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                timeout_count++;
                if (timeout_count > 3) {
                    printf("Timeout - fin de réception.\n");
                    break;
                }
                continue;
            }
            perror("Erreur de réception");
            break;
        }
        
        // Reset timeout counter on successful reception
        timeout_count = 0;
        
        // Check if this is the END message
        if (bytesReceived == 3 && strncmp(buffer, "END", 3) == 0) {
            printf("Message de fin 'END' reçu.\n");
            break;
        }
        
        // Write data to file
        fwrite(buffer, 1, bytesReceived, video);
        totalReceived += bytesReceived;
        printf("\rReçu: %zu bytes", totalReceived);
        fflush(stdout);
    }
    
    printf("\nVidéo reçue! Total: %zu bytes\n", totalReceived);
    fclose(video);
    close(sockfd);
}

/*------------------------------------------------------------------------------------------*/

void envoyerTCP() {
    int sockfd;
    struct sockaddr_in serverAddr;
    char buffer[BUFFER_SIZE] = "Message TCP du client";

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Erreur de socket TCP");
        exit(EXIT_FAILURE);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT_TCP);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Erreur de connexion TCP");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    send(sockfd, buffer, strlen(buffer), 0);
    printf("Message envoyé en TCP !\n");
    close(sockfd);
}

/*------------------------------------------------------------------------------------------*/

int main() {
    int choix;
    printf("Client - Choisissez le mode de communication :\n1. UDP\n2. TCP\n");
    scanf("%d", &choix);

    if (choix == 1 || choix == 2) {
        (*choixCommunicationClient[choix - 1])();
    } else {
        printf("Choix invalide.\n");
    }
    return 0;
}