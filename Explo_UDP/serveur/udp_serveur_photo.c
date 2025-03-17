/* server.c - Programme serveur UDP pour recevoir des images */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 8192  // Taille du buffer pour les fragments d'image
#define MAX_FILENAME_LEN 256

// Structure pour les en-têtes de paquet
typedef struct {
    uint32_t packet_id;    // Identifiant de paquet
    uint32_t total_size;   // Taille totale de l'image
    uint32_t chunk_size;   // Taille du fragment actuel
    uint32_t offset;       // Position du fragment dans l'image
    uint8_t is_last;       // Indique si c'est le dernier fragment
    char filename[MAX_FILENAME_LEN]; // Nom du fichier
} packet_header;

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE + sizeof(packet_header)];
    FILE *fp = NULL;
    
    // Création du socket UDP
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
    
    printf("Serveur UDP démarré sur le port %d...\n", PORT);
    
    // Variables pour suivre le transfert de l'image
    uint32_t total_received = 0;
    uint32_t expected_size = 0;
    char current_filename[MAX_FILENAME_LEN] = {0};
    
    while (1) {
        // Réception d'un paquet
        int bytes_received = recvfrom(sockfd, buffer, BUFFER_SIZE + sizeof(packet_header), 
                                     0, (struct sockaddr *)&client_addr, &addr_len);
        
        if (bytes_received < 0) {
            perror("Erreur lors de la réception");
            continue;
        }
        
        // Extraction de l'en-tête du paquet
        packet_header *header = (packet_header *)buffer;
        char *data = buffer + sizeof(packet_header);
        int data_size = bytes_received - sizeof(packet_header);
        
        // Si c'est le premier paquet d'une nouvelle image
        if (header->offset == 0) {
            // Fermer le fichier précédent s'il existe
            if (fp != NULL) {
                fclose(fp);
                fp = NULL;
            }
            
            // Créer un nouveau fichier pour l'image
            strncpy(current_filename, header->filename, MAX_FILENAME_LEN - 1);
            current_filename[MAX_FILENAME_LEN - 1] = '\0';  // Assure la terminaison
            
            // Ajoute un préfixe "received_" pour éviter d'écraser les fichiers d'origine
            char output_filename[MAX_FILENAME_LEN + 10];
            snprintf(output_filename, sizeof(output_filename), "received_%s", current_filename);
            
            fp = fopen(output_filename, "wb");
            if (fp == NULL) {
                perror("Erreur lors de la création du fichier");
                continue;
            }
            
            expected_size = header->total_size;
            total_received = 0;
            
            printf("Début de réception de l'image: %s (taille: %u octets)\n", 
                   current_filename, expected_size);
        }
        
        // Vérifier si le fichier est ouvert
        if (fp == NULL) {
            printf("Erreur: Aucun fichier ouvert pour écrire les données\n");
            continue;
        }
        
        // Écrire les données dans le fichier
        fseek(fp, header->offset, SEEK_SET);
        size_t written = fwrite(data, 1, data_size, fp);
        if (written != data_size) {
            perror("Erreur lors de l'écriture des données");
        }
        
        total_received += data_size;
        
        // Accusé de réception
        char ack[64];
        snprintf(ack, sizeof(ack), "ACK:%u", header->packet_id);
        sendto(sockfd, ack, strlen(ack), 0, (struct sockaddr *)&client_addr, addr_len);
        
        // Si c'est le dernier paquet
        if (header->is_last) {
            if (fp != NULL) {
                fclose(fp);
                fp = NULL;
            }
            
            if (total_received == expected_size) {
                printf("Image %s reçue avec succès (%u octets)\n", current_filename, total_received);
            } else {
                printf("Avertissement: Image %s reçue partiellement (%u/%u octets)\n", 
                       current_filename, total_received, expected_size);
            }
            
            // Envoi d'un accusé de réception final
            char final_ack[256];
            snprintf(final_ack, sizeof(final_ack), "TRANSFER_COMPLETE:%s:%u", 
                     current_filename, total_received);
            sendto(sockfd, final_ack, strlen(final_ack), 0, 
                   (struct sockaddr *)&client_addr, addr_len);
        }
    }
    
    // Ce code n'est jamais atteint à cause de la boucle infinie
    if (fp != NULL) {
        fclose(fp);
    }
    close(sockfd);
    return 0;
}