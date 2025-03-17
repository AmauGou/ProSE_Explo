#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <time.h>

#define PORT 12345  // Port d'écoute pour le serveur
#define BUFFER_SIZE 9000  // Taille du buffer UDP (maximum par paquet)
#define MAX_IMAGE_SIZE 10485760  // Taille maximale de l'image (10 Mo)
#define TIMEOUT_SECONDS 10  // Timeout pour chaque fragment

// Structure pour l'en-tête des fragments
typedef struct {
    uint32_t image_id;     // Identifiant unique de l'image
    uint32_t seq_num;      // Numéro de séquence du fragment
    uint32_t total_frags;  // Nombre total de fragments
    uint32_t frag_size;    // Taille du fragment en octets
    uint8_t is_last;       // Indique si c'est le dernier fragment
} ImageFragmentHeader;

// Structure pour stocker une image en cours de réception
typedef struct {
    uint32_t image_id;
    uint8_t *data;
    uint32_t total_size;
    uint32_t received_size;
    uint8_t *received_frags;  // Tableau de bits pour suivre les fragments reçus
    uint32_t total_frags;
    time_t last_update;
} ImageReceiver;

// Initialise la structure de réception d'image
ImageReceiver* init_image_receiver(uint32_t image_id, uint32_t total_frags) {
    ImageReceiver *receiver = malloc(sizeof(ImageReceiver));
    if (!receiver) return NULL;
    
    receiver->image_id = image_id;
    receiver->data = malloc(MAX_IMAGE_SIZE);
    receiver->total_size = 0;
    receiver->received_size = 0;
    receiver->received_frags = calloc((total_frags + 7) / 8, 1);  // Bitmap pour les fragments reçus
    receiver->total_frags = total_frags;
    receiver->last_update = time(NULL);
    
    return receiver;
}

// Vérifie si un fragment a été reçu
int is_fragment_received(ImageReceiver *receiver, uint32_t frag_num) {
    uint32_t byte_index = frag_num / 8;
    uint8_t bit_index = frag_num % 8;
    return (receiver->received_frags[byte_index] & (1 << bit_index)) != 0;
}

// Marque un fragment comme reçu
void mark_fragment_received(ImageReceiver *receiver, uint32_t frag_num) {
    uint32_t byte_index = frag_num / 8;
    uint8_t bit_index = frag_num % 8;
    receiver->received_frags[byte_index] |= (1 << bit_index);
}

// Vérifie si tous les fragments ont été reçus
int is_image_complete(ImageReceiver *receiver) {
    for (uint32_t i = 0; i < receiver->total_frags; i++) {
        if (!is_fragment_received(receiver, i)) {
            return 0;
        }
    }
    return 1;
}

// Libère les ressources de l'ImageReceiver
void free_image_receiver(ImageReceiver *receiver) {
    if (receiver) {
        free(receiver->data);
        free(receiver->received_frags);
        free(receiver);
    }
}

// Sauvegarde l'image complète dans un fichier
void save_image(ImageReceiver *receiver, const char* filename) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("Erreur lors de l'ouverture du fichier");
        return;
    }
    
    fwrite(receiver->data, 1, receiver->total_size, fp);
    fclose(fp);
    printf("Image sauvegardée sous %s (%u octets)\n", filename, receiver->total_size);
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t client_len = sizeof(client_addr);
    ImageReceiver *current_receiver = NULL;
    char filename[100];
    
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
    
    printf("Serveur UDP démarré sur le port %d, en attente d'images...\n", PORT);
    
    while (1) {
        // Configuration du timeout pour la fonction select
        fd_set read_fds;
        struct timeval tv;
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        tv.tv_sec = 1;  // Vérification toutes les secondes
        tv.tv_usec = 0;
        
        // Vérifier si des données sont disponibles
        int ready = select(sockfd + 1, &read_fds, NULL, NULL, &tv);
        
        // Vérifier le timeout pour l'image en cours
        time_t current_time = time(NULL);
        if (current_receiver && 
            difftime(current_time, current_receiver->last_update) > TIMEOUT_SECONDS) {
            printf("Timeout pour l'image ID %u, %u/%u fragments reçus\n", 
                   current_receiver->image_id, 
                   current_receiver->received_size / 8192, 
                   current_receiver->total_frags);
            free_image_receiver(current_receiver);
            current_receiver = NULL;
        }
        
        if (ready <= 0) continue;  // Timeout ou erreur, continuer la boucle
        
        // Réception des données
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, 
                        (struct sockaddr *)&client_addr, &client_len);
        
        if (n <= sizeof(ImageFragmentHeader)) {
            printf("Paquet trop petit reçu, ignoré\n");
            continue;
        }
        
        // Extraction de l'en-tête
        ImageFragmentHeader header;
        memcpy(&header, buffer, sizeof(header));
        
        // Affichage des informations du fragment
        printf("Fragment reçu: ID=%u, Seq=%u/%u, Taille=%u\n", 
               header.image_id, header.seq_num + 1, header.total_frags, header.frag_size);
        
        // Initialiser un nouveau récepteur si nécessaire
        if (!current_receiver || current_receiver->image_id != header.image_id) {
            if (current_receiver) {
                printf("Nouvelle image détectée, abandon de l'image précédente\n");
                free_image_receiver(current_receiver);
            }
            
            current_receiver = init_image_receiver(header.image_id, header.total_frags);
            if (!current_receiver) {
                perror("Erreur d'allocation mémoire");
                continue;
            }
            
            printf("Démarrage de la réception de l'image ID %u (%u fragments)\n", 
                   header.image_id, header.total_frags);
        }
        
        // Mettre à jour le timestamp
        current_receiver->last_update = time(NULL);
        
        // Si ce fragment a déjà été reçu, l'ignorer
        if (is_fragment_received(current_receiver, header.seq_num)) {
            printf("Fragment déjà reçu, ignoré\n");
            continue;
        }
        
        // Calculer l'offset pour ce fragment
        uint32_t offset = header.seq_num * 8192;  // Basé sur MAX_FRAG_SIZE du côté émetteur
        
        // Vérifier si l'offset est valide
        if (offset + header.frag_size > MAX_IMAGE_SIZE) {
            printf("Offset invalide, fragment ignoré\n");
            continue;
        }
        
        // Copier les données du fragment
        memcpy(current_receiver->data + offset, 
               buffer + sizeof(header), 
               header.frag_size);
        
        // Mettre à jour les compteurs et marquer comme reçu
        current_receiver->received_size += header.frag_size;
        mark_fragment_received(current_receiver, header.seq_num);
        
        // Mettre à jour la taille totale si c'est le dernier fragment
        if (header.is_last) {
            current_receiver->total_size = offset + header.frag_size;
        }
        
        // Vérifier si l'image est complète
        if (is_image_complete(current_receiver)) {
            printf("Image complète reçue! ID=%u, Taille=%u octets\n", 
                   current_receiver->image_id, current_receiver->total_size);
            
            // Générer un nom de fichier unique
            sprintf(filename, "received_image_%u.jpg", current_receiver->image_id);
            
            // Sauvegarder l'image
            save_image(current_receiver, filename);
            
            // Libérer les ressources
            free_image_receiver(current_receiver);
            current_receiver = NULL;
        }
    }
    
    close(sockfd);
    return 0;
}
