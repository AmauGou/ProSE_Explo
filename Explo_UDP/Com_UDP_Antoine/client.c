// sender.c - Code client pour envoyer des images JPEG via UDP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <time.h>

#define PORT 12345
#define SERVER_IP "127.0.0.1"
#define MAX_FRAG_SIZE 8192  // 8 Ko par fragment

// Structure pour l'en-tête des fragments
typedef struct {
    uint32_t image_id;     // Identifiant unique de l'image
    uint32_t seq_num;      // Numéro de séquence du fragment
    uint32_t total_frags;  // Nombre total de fragments
    uint32_t frag_size;    // Taille du fragment en octets
    uint8_t is_last;       // Indique si c'est le dernier fragment
} ImageFragmentHeader;

// Envoie une image JPEG via UDP
void send_jpeg_image(int sockfd, struct sockaddr_in *dest_addr, const char *image_path) {
    // Ouvrir et lire l'image
    FILE *fp = fopen(image_path, "r");
    if (!fp) {
        perror("Impossible d'ouvrir l'image");
        return;
    }
    
    // Déterminer la taille de l'image
    fseek(fp, 0, SEEK_END);
    long image_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    printf("Taille de l'image %s: %ld octets\n", image_path, image_size);
    
    // Allouer la mémoire pour l'image
    uint8_t *image_data = malloc(image_size);
    if (!image_data) {
        perror("Erreur d'allocation mémoire");
        fclose(fp);
        return;
    }
    
    // Lire l'image en mémoire
    size_t bytes_read = fread(image_data, 1, image_size, fp);
    fclose(fp);
    
    if (bytes_read != image_size) {
        perror("Erreur de lecture du fichier");
        free(image_data);
        return;
    }
    
    // Paramètres de fragmentation
    uint32_t total_frags = (image_size + MAX_FRAG_SIZE - 1) / MAX_FRAG_SIZE;
    uint32_t image_id = (uint32_t)time(NULL);  // Utiliser le timestamp comme ID
    
    printf("Fragmentation de l'image en %u fragments de %u octets max\n", 
           total_frags, MAX_FRAG_SIZE);
    
    // Buffer pour stocker l'en-tête + les données
    uint8_t *packet = malloc(sizeof(ImageFragmentHeader) + MAX_FRAG_SIZE);
    if (!packet) {
        perror("Erreur d'allocation mémoire pour le packet");
        free(image_data);
        return;
    }
    
    // Envoyer chaque fragment
    for (uint32_t i = 0; i < total_frags; i++) {
        uint32_t offset = i * MAX_FRAG_SIZE;
        uint32_t current_frag_size = (i == total_frags - 1) ? // IF-ELSE STATEMENT
            (image_size - offset) : MAX_FRAG_SIZE;
        
        // Préparer l'en-tête
        ImageFragmentHeader header = {
            .image_id = image_id,
            .seq_num = i,
            .total_frags = total_frags,
            .frag_size = current_frag_size,
            .is_last = (i == total_frags - 1) ? 1 : 0
        };
        
        // Copier l'en-tête dans le packet
        memcpy(packet, &header, sizeof(header));
        
        // Copier les données dans le packet
        memcpy(packet + sizeof(header), image_data + offset, current_frag_size);
        
        // Envoyer le fragment
        ssize_t sent_bytes = sendto(sockfd, packet, sizeof(header) + current_frag_size, 0,
                                   (struct sockaddr *)dest_addr, sizeof(*dest_addr));
        
        if (sent_bytes < 0) {
            perror("Erreur lors de l'envoi");
            continue;
        }
        
        printf("Fragment %u/%u envoyé (%u octets)\n", 
               i + 1, total_frags, (unsigned int)sent_bytes);
        
        // Ajouter un court délai pour éviter la congestion
        usleep(10000);  // 10ms de délai
    }
    
    free(packet);
    free(image_data);
    printf("Image envoyée avec succès! ID=%u\n", image_id);
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in server_addr;
    
    // Vérifier les arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <chemin_image.jpg>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // Création du socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Erreur lors de la création du socket");
        exit(EXIT_FAILURE);
    }
    
    // Configuration de l'adresse du serveur
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    // Conversion de l'adresse IP en format binaire
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Adresse invalide / non supportée");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    printf("Envoi de l'image %s au serveur %s:%d\n", argv[1], SERVER_IP, PORT);
    
    // Envoyer l'image
    send_jpeg_image(sockfd, &server_addr, argv[1]);
    
    close(sockfd);
    return 0;
}