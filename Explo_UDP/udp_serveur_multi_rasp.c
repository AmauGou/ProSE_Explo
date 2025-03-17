#include <stdio.h>      // Pour les fonctions d'entrée/sortie standard
#include <stdlib.h>     // Pour malloc(), free(), exit()
#include <string.h>     // Pour memset(), memcpy(), strlen()
#include <unistd.h>     // Pour close()
#include <arpa/inet.h>  // Pour les fonctions réseau et structures sockaddr
#include <pthread.h>    // Pour les fonctions de threading

#define PORT 8080       // Port d'écoute du serveur
#define BUFFER_SIZE 1024 // Taille du buffer pour les messages

// Structure pour passer les informations du client au thread
// Cette structure contient toutes les données nécessaires pour qu'un thread traite une requête
typedef struct {
    struct sockaddr_in client_addr; // Stocke l'adresse IP et le port du client
    socklen_t addr_len;             // Taille de la structure d'adresse
    char message[BUFFER_SIZE];      // Le message reçu du client
    int message_len;                // Longueur du message
    int sockfd;                     // Descripteur du socket pour envoyer la réponse
} client_data;

// Fonction exécutée par chaque thread pour traiter un client
// Cette fonction est appelée quand un nouveau thread est créé
void* handle_client(void* arg) {
    // Conversion du paramètre générique en structure client_data
    client_data* data = (client_data*)arg;
    
    // Ajout d'un caractère nul à la fin du message pour le traiter comme une chaîne
    data->message[data->message_len] = '\0';
    
    // Affichage du message avec l'ID du thread actuel
    printf("Thread %lu: Message reçu: %s\n", pthread_self(), data->message);
    
    // Préparation de la réponse - inclut le message original et l'ID du thread
    char response[BUFFER_SIZE];

    /*
    int snprintf(char *str, size_t size, const char *format, ...);

    Voici ce que font ses paramètres :

    str : Le buffer de destination où la chaîne formatée sera écrite
    size : La taille maximale du buffer (en octets), y compris le caractère nul de fin
    format : La chaîne de format, similaire à celle utilisée par printf()
    ... : Les arguments variables correspondant à la chaîne de format
    */

    snprintf(response, BUFFER_SIZE, "Message \"%s\" bien reçu! Traité par le thread %lu", 
             data->message, pthread_self());
    
    // Envoi de la réponse au client en utilisant l'adresse stockée dans la structure
    sendto(data->sockfd, response, strlen(response), 0, 
           (struct sockaddr*)&data->client_addr, data->addr_len);
    
    printf("Thread %lu: Réponse envoyée au client.\n", pthread_self());
    
    // Libération de la mémoire allouée pour les données du client
    // C'est important pour éviter les fuites de mémoire
    free(data);
    
    // Fin du thread
    pthread_exit(NULL);
}

int main() {
    int sockfd;                     // Descripteur du socket
    struct sockaddr_in server_addr; // Structure pour l'adresse du serveur
    struct sockaddr_in client_addr; // Structure pour l'adresse du client
    char buffer[BUFFER_SIZE];       // Buffer temporaire pour recevoir les messages
    socklen_t addr_len = sizeof(client_addr); // Taille de la structure d'adresse client
    pthread_t thread_id;            // ID pour chaque thread créé
    
    // Création du socket UDP
    // AF_INET: Utilisation du protocole IPv4
    // SOCK_DGRAM: Type de socket pour UDP (datagrammes)
    // 0: Utilisation du protocole par défaut pour ce type de socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Erreur lors de la création du socket");
        exit(EXIT_FAILURE);
    }
    
    // Initialisation de la structure d'adresse du serveur à zéro
    memset(&server_addr, 0, sizeof(server_addr));
    
    // Configuration de l'adresse du serveur
    server_addr.sin_family = AF_INET;           // Famille d'adresses IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY;   // Accepte les connexions sur toutes les interfaces
    server_addr.sin_port = htons(PORT);         // Conversion du port en format réseau (big-endian)
    
    // Liaison du socket à l'adresse et au port configurés
    // Cette étape est nécessaire pour que le serveur puisse recevoir des messages
    if (bind(sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Échec de la liaison");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    printf("Serveur UDP multithreadé en attente sur le port %d...\n", PORT);
    
    // Boucle infinie pour traiter les requêtes entrantes
    while (1) {
        // Réception d'un message du client
        // recvfrom est bloquant: il attend jusqu'à ce qu'un message arrive
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, 
                        (struct sockaddr *)&client_addr, &addr_len);
        
        // Vérification d'erreur lors de la réception
        if (n < 0) {
            perror("Erreur lors de la réception");
            continue; // Continue la boucle en cas d'erreur       
        }
        
        // Allocation de mémoire pour les données du client
        // Ces données seront passées au thread
        client_data* data = calloc(sizeof(client_data),1);
        if (data == NULL) {
            perror("Erreur d'allocation mémoire");
            continue; // Continue la boucle en cas d'erreur d'allocation
        }
        
        // Copie des informations du client dans la structure
        memcpy(&data->client_addr, &client_addr, sizeof(client_addr));
        data->addr_len = addr_len;
        memcpy(data->message, buffer, n);   // Copie du message reçu
        data->message_len = n;              // Stockage de la longueur du message
        data->sockfd = sockfd;              // Partage du descripteur de socket
        
        // Création d'un nouveau thread pour traiter ce client
        // Le thread exécutera la fonction handle_client avec les données du client
        if (pthread_create(&thread_id, NULL, handle_client, (void*)data) != 0) {
            perror("Erreur lors de la création du thread");
            free(data); // Libération de la mémoire en cas d'erreur
            continue;
        }
        
        // Détachement du thread
        // Cela permet au thread de libérer ses ressources automatiquement quand il se termine
        // sans avoir besoin d'appeler pthread_join
        pthread_detach(thread_id);
        
        printf("Nouveau thread %lu créé pour traiter le client.\n", thread_id);
    }
    
    // Ce code n'est jamais atteint à cause de la boucle infinie,
    // mais il est bon de l'inclure pour la propreté du code
    close(sockfd);
    return 0;
}