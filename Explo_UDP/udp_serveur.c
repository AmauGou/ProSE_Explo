#include <stdio.h>      // Bibliothèque standard pour les entrées/sorties
#include <stdlib.h>     // Bibliothèque pour les fonctions standard comme exit()
#include <string.h>     // Bibliothèque pour manipuler les chaînes de caractères
#include <unistd.h>     // Bibliothèque pour les fonctions POSIX comme close()
#include <arpa/inet.h>  // Bibliothèque pour les fonctionnalités réseau (sockets)

#define PORT 8080          // Définition du port sur lequel le serveur écoute
#define BUFFER_SIZE 1024   // Taille maximale du buffer pour recevoir les messages

int main() {
    int sockfd;  // Descripteur de socket
    char buffer[BUFFER_SIZE];  // Buffer pour stocker les données reçues
    struct sockaddr_in server_addr, client_addr;  // Structures pour stocker les adresses du serveur et du client
    socklen_t addr_len = sizeof(client_addr);  // Taille de l'adresse du client

    // Création du socket UDP
    // AF_INET : famille d'adresses IPv4
    // SOCK_DGRAM : type de socket pour UDP
    // 0 : utilisation du protocole par défaut (UDP)
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Erreur lors de la création du socket");
        exit(EXIT_FAILURE);
    }

    // Initialisation de la structure d'adresse du serveur
    memset(&server_addr, 0, sizeof(server_addr)); // Mise à zéro de la structure
    server_addr.sin_family = AF_INET;            // Utilisation de la famille d'adresses IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY;    // Le serveur accepte les connexions depuis n'importe quelle adresse
    server_addr.sin_port = htons(PORT);          // Conversion du port en format réseau

    // Liaison du socket à l'adresse et au port définis
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Échec de la liaison");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Serveur UDP en attente de messages sur le port %d...\n", PORT);

    while (1) {
        // Réception d'un message du client
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
        if (n < 0) {
            perror("Erreur lors de la réception");
            continue;
        }
        buffer[n] = '\0';  // Ajout d'un caractère de fin pour gérer le buffer comme une chaîne de caractères
        printf("Message reçu: %s\n", buffer);

        // Préparation de la réponse
        char *response = "Message reçu avec succès!";
        
        // Envoi de la réponse au client
        sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)&client_addr, addr_len);
        printf("Réponse envoyée au client.\n");
    }

    // Fermeture du socket (ne sera jamais atteint dans ce cas car boucle infinie)
    close(sockfd);
    return 0;
}
