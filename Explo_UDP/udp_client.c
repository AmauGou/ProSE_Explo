#include <stdio.h>      // Bibliothèque standard pour les entrées/sorties
#include <stdlib.h>     // Bibliothèque pour les fonctions standard comme exit()
#include <string.h>     // Bibliothèque pour manipuler les chaînes de caractères
#include <unistd.h>     // Bibliothèque pour les fonctions POSIX comme close()
#include <arpa/inet.h>  // Bibliothèque pour les fonctionnalités réseau (sockets)

#define SERVER_IP "127.0.0.1"  // Adresse IP du serveur (localhost) à changer par celle du serveur distant 
#define PORT 8080              // Port du serveur
#define BUFFER_SIZE 1024        // Taille maximale du buffer pour recevoir les messages

int main() {
    int sockfd;  // Descripteur de socket
    struct sockaddr_in server_addr;  // Structure pour stocker l'adresse du serveur
    char buffer[BUFFER_SIZE];  // Buffer pour stocker les messages
    socklen_t addr_len = sizeof(server_addr);  // Taille de l'adresse du serveur

    // Création du socket UDP
    // AF_INET : famille d'adresses IPv4
    // SOCK_DGRAM : type de socket pour UDP
    // 0 : utilisation du protocole par défaut (UDP)
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Erreur lors de la création du socket");
        exit(EXIT_FAILURE);
    }

    // Configuration de l'adresse du serveur
    memset(&server_addr, 0, sizeof(server_addr)); // Mise à zéro de la structure
    server_addr.sin_family = AF_INET;            // Utilisation de la famille d'adresses IPv4
    server_addr.sin_port = htons(PORT);          // Conversion du port en format réseau
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);  // Conversion de l'adresse IP en format réseau

    // Message à envoyer au serveur
    char *message = "Hello, serveur UDP!";
    
    // Envoi du message au serveur
    if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&server_addr, addr_len) < 0) {
        perror("Erreur lors de l'envoi du message");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("Message envoyé au serveur: %s\n", message);

    // Réception de la réponse du serveur
    int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &addr_len);
    if (n < 0) {
        perror("Erreur lors de la réception de la réponse");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    buffer[n] = '\0';  // Ajout d'un caractère de fin pour gérer le buffer comme une chaîne de caractères
    printf("Réponse du serveur: %s\n", buffer);

    // Fermeture du socket
    close(sockfd);
    return 0;
}
