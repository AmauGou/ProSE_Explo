/* client.c - Programme client UDP pour envoyer des images */
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
#include <time.h>

#define SERVER_IP "127.0.0.1"  // Adresse IP du serveur à modifier selon vos besoins
#define PORT 8080              // Port du serveur
#define BUFFER_SIZE 8192       // Taille du buffer pour les fragments d'image
#define MAX_FILENAME_LEN 256
#define MAX_RETRIES 5          // Nombre maximum de tentatives de renvoi
#define TIMEOUT_SEC 2          // Délai d'attente en secondes pour les ACKs

// Structure pour les en-têtes de paquet
typedef struct {
    uint32_t packet_id;    // Identifiant de paquet
    uint32_t total_size;   // Taille totale de l'image
    uint32_t chunk_size;   // Taille du fragment actuel
    uint32_t offset;       // Position du fragment dans l'image
    uint8_t is_last;       // Indique si c'est le dernier fragment
    char filename[MAX_FILENAME_LEN]; // Nom du fichier
} packet_header;

// Fonction pour envoyer un fichier image
int send_image(int sockfd, struct sockaddr_in *server_addr, const char *filename) {
    FILE *fp;
    char buffer[BUFFER_SIZE + sizeof(packet_header)];
    uint32_t packet_id = 0;
    struct stat file_stat;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    
    // Ouvrir le fichier image
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Erreur lors de l'ouverture du fichier");
        return -1;
    }
   
    // Obtenir la taille du fichier
    if (stat(filename, &file_stat) != 0) {
        perror("Erreur lors de l'obtention de la taille du fichier");
        fclose(fp);
        return -1;
    }
    
    // Extraire le nom de base du fichier (sans le chemin)
    const char *basename = strrchr(filename, '/');
    if (stat(filename, &file_stat) != 0) {
        perror("Erreur lors de l'obtention de la taille du fichier");
        printf("Code d'erreur: %d, Message: %s\n", errno, strerror(errno));
        fclose(fp);
        return -1;
    }   
    
    printf("Envoi de l'image: %s (taille: %ld octets)\n", basename, file_stat.st_size);
    
    // Configurer le socket pour un timeout
    struct timeval tv;
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Erreur lors de la configuration du timeout");
    }
    
    uint32_t offset = 0;
    uint32_t bytes_left = file_stat.st_size;
    
    // Envoyer le fichier par fragments
    while (bytes_left > 0) {
        // Préparer l'en-tête du paquet
        packet_header *header = (packet_header *)buffer;
        header->packet_id = packet_id++;
        header->total_size = file_stat.st_size;
        header->offset = offset;
        header->chunk_size = (bytes_left > BUFFER_SIZE) ? BUFFER_SIZE : bytes_left;
        header->is_last = (bytes_left <= BUFFER_SIZE) ? 1 : 0;
        strncpy(header->filename, basename, MAX_FILENAME_LEN - 1);
        header->filename[MAX_FILENAME_LEN - 1] = '\0';  // Assure la terminaison
        
        // Positionner le pointeur de fichier
        fseek(fp, offset, SEEK_SET);
        
        // Lire les données du fichier
        size_t bytes_read = fread(buffer + sizeof(packet_header), 1, header->chunk_size, fp);
        if (bytes_read != header->chunk_size) {
            if (feof(fp)) {
                // Fin du fichier atteinte, ajuster la taille du chunk
                header->chunk_size = bytes_read;
                header->is_last = 1;
            } else {
                perror("Erreur lors de la lecture du fichier");
                fclose(fp);
                return -1;
            }
        }
        
        int retry_count = 0;
        int ack_received = 0;
        
        // Boucle de tentatives d'envoi avec accusé de réception
        while (!ack_received && retry_count < MAX_RETRIES) {
            // Envoyer le paquet
            if (sendto(sockfd, buffer, sizeof(packet_header) + header->chunk_size, 0,
                      (struct sockaddr *)server_addr, addr_len) < 0) {
                perror("Erreur lors de l'envoi du paquet");
                fclose(fp);
                return -1;
            }
            
            printf("Paquet %u envoyé (offset: %u, taille: %u, dernier: %d)\n", 
                   header->packet_id, header->offset, header->chunk_size, header->is_last);
            
            // Attendre l'accusé de réception
            char ack_buffer[256];
            ssize_t ack_len = recvfrom(sockfd, ack_buffer, sizeof(ack_buffer), 0, NULL, NULL);
            
            if (ack_len > 0) {
                ack_buffer[ack_len] = '\0';
                
                // Vérifier si c'est l'ACK attendu
                char expected_ack[64];
                snprintf(expected_ack, sizeof(expected_ack), "ACK:%u", header->packet_id);
                
                if (strncmp(ack_buffer, expected_ack, strlen(expected_ack)) == 0) {
                    ack_received = 1;
                    printf("ACK reçu pour le paquet %u\n", header->packet_id);
                } else if (strncmp(ack_buffer, "TRANSFER_COMPLETE", 17) == 0) {
                    printf("Transfert terminé: %s\n", ack_buffer);
                    if (header->is_last) {
                        ack_received = 1;
                    }
                }
            } else {
                retry_count++;
                printf("Délai d'attente dépassé, nouvelle tentative %d/%d\n", 
                       retry_count, MAX_RETRIES);
            }
        }
        
        if (!ack_received) {
            printf("Échec de l'envoi du paquet après %d tentatives\n", MAX_RETRIES);
            fclose(fp);
            return -1;
        }
        
        // Mettre à jour les compteurs
        offset += header->chunk_size;
        bytes_left -= header->chunk_size;
    }
    
    fclose(fp);
    
    // Attendre l'ACK final si nécessaire
    if (packet_id > 0) {
        char final_ack[256];
        int retry_count = 0;
        int final_ack_received = 0;
        
        while (!final_ack_received && retry_count < MAX_RETRIES) {
            ssize_t ack_len = recvfrom(sockfd, final_ack, sizeof(final_ack), 0, NULL, NULL);
            
            if (ack_len > 0) {
                final_ack[ack_len] = '\0';
                if (strncmp(final_ack, "TRANSFER_COMPLETE", 17) == 0) {
                    printf("Transfert complet confirmé: %s\n", final_ack);
                    final_ack_received = 1;
                }
            } else {
                retry_count++;
                // Ce n'est pas critique si nous ne recevons pas l'ACK final
                // car les paquets individuels ont déjà été confirmés
                if (retry_count >= MAX_RETRIES) {
                    printf("Pas d'ACK final reçu, mais le transfert semble complet\n");
                    break;
                }
            }
        }
    }
    
    printf("Image envoyée avec succès: %s\n", basename);
    return 0;
}


int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in server_addr;
    
    // Vérifier les arguments
    if (argc != 2) {
        printf("Usage: %s <nom_du_fichier_image>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    // Création du socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Erreur lors de la création du socket");
        return EXIT_FAILURE;
    }
    
    // Configuration de l'adresse du serveur
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    // Convertir l'adresse IP en format binaire
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Erreur lors de la conversion de l'adresse IP");
        close(sockfd);
        return EXIT_FAILURE;
    }
    
    // Envoyer l'image
    if (send_image(sockfd, &server_addr, argv[1]) < 0) {
        printf("Échec de l'envoi de l'image\n");
    }
    
    close(sockfd);
    return 0;
}