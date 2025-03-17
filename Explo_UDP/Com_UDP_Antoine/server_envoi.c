#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <time.h>

#define PORT 12345
#define BUFFER_SIZE 9000  // Pour accueillir l'en-tête + données (8Ko + marge)
#define MAX_IMAGE_SIZE 10485760  // 10 Mo max par image
#define MAX_FRAG_SIZE 8192  // Taille maximale d'un fragment (8 Ko)
#define TIMEOUT_SECONDS 10  // Timeout pour une image complète

// Structure pour l'en-tête des fragments
typedef struct {
    uint32_t image_id;     // Identifiant unique de l'image
    uint32_t seq_num;      // Numéro de séquence du fragment
    uint32_t total_frags;  // Nombre total de fragments
    uint32_t frag_size;    // Taille du fragment en octets
    uint8_t is_last;       // Indique si c'est le dernier fragment
} ImageFragmentHeader;

// Structure pour stocker une image
typedef struct {
    uint32_t image_id;
    uint8_t *data;
    uint32_t size;
    uint32_t total_frags;
} ImageToSend;

// Fonction pour envoyer un fragment
int send_image_fragment(int sockfd, struct sockaddr_in *client_addr, ImageToSend *image, uint32_t seq_num) {
    // Calculer l'offset du fragment
    uint32_t offset = seq_num * MAX_FRAG_SIZE;
    uint32_t frag_size = (offset + MAX_FRAG_SIZE <= image->size) ? MAX_FRAG_SIZE : (image->size - offset);
    
    // Créer l'en-tête du fragment
    ImageFragmentHeader header;
    header.image_id = image->image_id;
    header.seq_num = seq_num;
    header.total_frags = image->total_frags;
    header.frag_size = frag_size;
    header.is_last = (offset + frag_size >= image->size);
    
    // Créer un tampon pour le fragment (en-tête + données)
    uint8_t buffer[BUFFER_SIZE];
    memcpy(buffer, &header, sizeof(header));
    memcpy(buffer + sizeof(header), image->data + offset, frag_size);
    
    // Envoyer le fragment
    socklen_t client_len = sizeof(*client_addr);
    ssize_t sent_size = sendto(sockfd, buffer, sizeof(header) + frag_size, 0, 
                               (struct sockaddr *)client_addr, client_len);
    if (sent_size < 0) {
        perror("Erreur lors de l'envoi du fragment");
        return -1;
    }
    
    printf("Fragment envoyé: ID=%u, Seq=%u/%u, Taille=%u\n", 
           header.image_id, header.seq_num + 1, header.total_frags, frag_size);
    
    return 0;
}

// Fonction pour charger l'image depuis un fichier
ImageToSend* load_image(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Erreur lors de l'ouverture du fichier");
        return NULL;
    }
    
    fseek(fp, 0, SEEK_END);
    uint32_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    // Allouer la mémoire pour l'image
    ImageToSend *image = malloc(sizeof(ImageToSend));
    if (!image) {
        perror("Erreur d'allocation mémoire");
        fclose(fp);
        return NULL;
    }
    
    image->image_id = (uint32_t)time(NULL);  // Utiliser un timestamp comme ID unique
    image->data = malloc(size);
    if (!image->data) {
        perror("Erreur d'allocation mémoire pour les données de l'image");
        free(image);
        fclose(fp);
        return NULL;
    }
    
    // Lire l'image dans la mémoire
    fread(image->data, 1, size, fp);
    fclose(fp);
    
    image->size = size;
    image->total_frags = (size + MAX_FRAG_SIZE - 1) / MAX_FRAG_SIZE;  // Nombre de fragments
    
    return image;
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    
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
    
    // Attachement du socket à l'adresse et au port spécifiés
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erreur lors du bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    printf("Serveur UDP démarré sur le port %d, en attente d'un client...\n", PORT);
    
    // Attente de la connexion du client
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, 
                     (struct sockaddr *)&client_addr, &client_len);
    if (n < 0) {
        perror("Erreur lors de la réception des données");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    printf("Client connecté, envoi de l'image...\n");
    
    // Charger l'image depuis le fichier
    ImageToSend *image = load_image("image.png");  // Remplacer par le chemin de votre image
    if (!image) {
        printf("Échec du chargement de l'image\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    // Envoyer les fragments de l'image
    for (uint32_t i = 0; i < image->total_frags; i++) {
        if (send_image_fragment(sockfd, &client_addr, image, i) < 0) {
            free(image->data);
            free(image);
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        usleep(100000);  // Pause de 100ms pour éviter la surcharge réseau
    }
    
    printf("Image envoyée avec succès!\n");
    
    // Libérer les ressources
    free(image->data);
    free(image);
    close(sockfd);
    return 0;
}
