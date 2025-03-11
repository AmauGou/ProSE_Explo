#include <stdio.h>      
#include <stdlib.h>     
#include <string.h>     
#include <unistd.h>     
#include <arpa/inet.h>  

#define PORT 8080          // Port d'écoute du serveur
#define BUFFER_SIZE 1024   // Taille du buffer

/*-----------------------------------------------------------*/
/* Compilez le serveur avec : gcc udp_server.c -o udp_server */
/*-----------------------------------------------------------*/

int main() {
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Création du socket UDP
    // Création du socket UDP
    // AF_INET : famille d'adresses IPv4
    // SOCK_DGRAM : type de socket pour UDP
    // 0 : utilisation du protocole par défaut (UDP)
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Erreur lors de la création du socket");
        exit(EXIT_FAILURE);
    }

    // Configuration de l'adresse du serveur
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Liaison du socket à l'adresse et au port
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Échec de la liaison");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Serveur UDP en attente sur le port %d...\n", PORT);

    while (1) {
        // Réception d'un message du client
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
        if (n < 0) {
            perror("Erreur lors de la réception");
            continue;
        }
        
        // Ajout d'un caractère de fin pour gérer le buffer comme une chaîne de caractères
        buffer[n] = '\0';
        printf("Message reçu: %s\n", buffer);

        // Réponse au client
        char *response = "Message reçu par Raspberry Pi !";

        // Envoi de la réponse au client
        sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)&client_addr, addr_len);
        printf("Réponse envoyée au client.\n");
    }

    // Fermeture du socket (ne sera jamais atteint dans ce cas car boucle infinie)
    close(sockfd);
    return 0;
}
