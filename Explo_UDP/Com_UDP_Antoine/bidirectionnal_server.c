// bidirectional_server.c - Serveur UDP capable de recevoir et envoyer des images JPEG

#define _GNU_SOURCE


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>  // Pour mkdir
#include <sys/types.h> // Types supplémentaires pour mkdir

#define PORT 8888
#define BUFFER_SIZE 9000  // Pour accueillir l'en-tête + données (8Ko + marge)
#define MAX_IMAGE_SIZE 10485760  // 10 Mo max par image
#define TIMEOUT_SECONDS 10  // Timeout pour une image complète
#define MAX_FRAG_SIZE 8192  // 8 Ko par fragment
#define MAX_CLIENTS 10      // Nombre maximum de clients à mémoriser
#define CMD_BUFFER_SIZE 1024 // Taille du buffer pour les commandes

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

// Structure pour stocker les informations des clients
typedef struct {
    struct sockaddr_in addr;
    socklen_t addr_len;
    time_t last_seen;
    char client_id[50];
} ClientInfo;

// Variables globales
int sockfd;
ClientInfo clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int running = 1;

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

// Initialisation du tableau des clients
void init_clients() {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        memset(&clients[i].addr, 0, sizeof(struct sockaddr_in));
        clients[i].last_seen = 0;
        clients[i].client_id[0] = '\0';
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Ajoute ou met à jour un client dans le tableau
void update_client(struct sockaddr_in *client_addr, socklen_t addr_len) {
    char client_id[50];
    snprintf(client_id, sizeof(client_id), "%s:%d", 
             inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
    
    pthread_mutex_lock(&clients_mutex);
    
    // Chercher si le client existe déjà
    int empty_slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].last_seen == 0) {
            if (empty_slot == -1) empty_slot = i;
            continue;
        }
        
        if (clients[i].addr.sin_addr.s_addr == client_addr->sin_addr.s_addr && 
            clients[i].addr.sin_port == client_addr->sin_port) {
            // Client trouvé, mise à jour
            clients[i].last_seen = time(NULL);
            pthread_mutex_unlock(&clients_mutex);
            return;
        }
    }
    
    // Si client non trouvé et qu'il y a un emplacement libre
    if (empty_slot != -1) {
        memcpy(&clients[empty_slot].addr, client_addr, sizeof(struct sockaddr_in));
        clients[empty_slot].addr_len = addr_len;
        clients[empty_slot].last_seen = time(NULL);
        strncpy(clients[empty_slot].client_id, client_id, sizeof(clients[empty_slot].client_id));
        printf("Nouveau client enregistré: %s\n", client_id);
    } else {
        printf("Tableau de clients plein, impossible d'enregistrer %s\n", client_id);
    }
    
    pthread_mutex_unlock(&clients_mutex);
}

// Nettoie les clients inactifs
void cleanup_clients() {
    time_t current_time = time(NULL);
    
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].last_seen > 0 && 
            difftime(current_time, clients[i].last_seen) > 300) { // 5 minutes d'inactivité
            printf("Client expiré: %s\n", clients[i].client_id);
            clients[i].last_seen = 0;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Affiche la liste des clients connectés
void list_clients() {
    printf("\nClients connectés:\n");
    printf("------------------\n");
    
    pthread_mutex_lock(&clients_mutex);
    int active_clients = 0;
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].last_seen > 0) {
            active_clients++;
            printf("%d. %s (dernière activité: %ld secondes)\n", 
                   i + 1, 
                   clients[i].client_id, 
                   time(NULL) - clients[i].last_seen);
        }
    }
    
    if (active_clients == 0) {
        printf("Aucun client actif\n");
    }
    
    pthread_mutex_unlock(&clients_mutex);
    printf("------------------\n");
}

// Liste les images disponibles dans le dossier spécifié
void list_images(const char *directory) {
    DIR *dir;
    struct dirent *entry;
    
    printf("\nImages disponibles dans %s:\n", directory);
    printf("----------------------------\n");
    
    dir = opendir(directory);
    if (!dir) {
        perror("Impossible d'ouvrir le dossier");
        return;
    }
    
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            // Vérifier si c'est un fichier JPEG
            char *ext = strrchr(entry->d_name, '.');
            if (ext && (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0)) {
                printf("%d. %s\n", ++count, entry->d_name);
            }
        }
    }
    
    if (count == 0) {
        printf("Aucune image JPEG trouvée\n");
    }
    
    closedir(dir);
    printf("----------------------------\n");
}

// Envoie une image JPEG via UDP
void send_jpeg_image(int sockfd, struct sockaddr_in *dest_addr, socklen_t addr_len, const char *image_path) {
    // Ouvrir et lire l'image
    FILE *fp = fopen(image_path, "rb");
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
        uint32_t current_frag_size = (i == total_frags - 1) ? 
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
                                   (struct sockaddr *)dest_addr, addr_len);
        
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

// Thread pour lire les commandes de l'utilisateur
void* command_thread(void *arg) {
    char cmd_buffer[CMD_BUFFER_SIZE];
    char img_path[256];
    int client_idx;
    
    printf("\nCommandes disponibles:\n");
    printf("- list_clients : Affiche la liste des clients connectés\n");
    printf("- list_images [dossier] : Liste les images JPEG dans le dossier spécifié\n");
    printf("- send_image [client_idx] [chemin_image] : Envoie une image au client spécifié\n");
    printf("- broadcast_image [chemin_image] : Envoie une image à tous les clients\n");
    printf("- quit : Quitte le serveur\n");
    
    while (running) {
        printf("\nEntrez une commande: ");
        fflush(stdout);
        
        if (fgets(cmd_buffer, CMD_BUFFER_SIZE, stdin) == NULL) {
            break;
        }
        
        // Supprimer le caractère de nouvelle ligne
        cmd_buffer[strcspn(cmd_buffer, "\n")] = 0;
        
        if (strcmp(cmd_buffer, "list_clients") == 0) {
            list_clients();
        }
        else if (strncmp(cmd_buffer, "list_images", 11) == 0) {
            char directory[256] = ".";  // Dossier par défaut
            
            // Extraction du dossier s'il est spécifié
            sscanf(cmd_buffer + 11, " %255s", directory);
            
            list_images(directory);
        }
        else if (strncmp(cmd_buffer, "send_image", 10) == 0) {
            int client_idx = -1;
            char image_path[256] = "";
            
            if (sscanf(cmd_buffer + 10, " %d %255s", &client_idx, image_path) == 2) {
                client_idx--; // Ajuster l'index (l'affichage commence à 1, les tableaux à 0)
                
                pthread_mutex_lock(&clients_mutex);
                if (client_idx >= 0 && client_idx < MAX_CLIENTS && clients[client_idx].last_seen > 0) {
                    struct sockaddr_in client_addr = clients[client_idx].addr;
                    socklen_t addr_len = clients[client_idx].addr_len;
                    
                    printf("Envoi de l'image %s au client %s...\n", 
                           image_path, clients[client_idx].client_id);
                    
                    pthread_mutex_unlock(&clients_mutex);
                    send_jpeg_image(sockfd, &client_addr, addr_len, image_path);
                } else {
                    pthread_mutex_unlock(&clients_mutex);
                    printf("Index client invalide ou client inactif\n");
                }
            } else {
                printf("Syntaxe incorrecte. Usage: send_image [client_idx] [chemin_image]\n");
            }
        }
        else if (strncmp(cmd_buffer, "broadcast_image", 15) == 0) {
            char image_path[256] = "";
            
            if (sscanf(cmd_buffer + 15, " %255s", image_path) == 1) {
                printf("Diffusion de l'image %s à tous les clients...\n", image_path);
                
                pthread_mutex_lock(&clients_mutex);
                int sent_count = 0;
                
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].last_seen > 0) {
                        printf("Envoi au client %s...\n", clients[i].client_id);
                        send_jpeg_image(sockfd, &clients[i].addr, clients[i].addr_len, image_path);
                        sent_count++;
                    }
                }
                
                pthread_mutex_unlock(&clients_mutex);
                printf("Image diffusée à %d clients\n", sent_count);
                
                if (sent_count == 0) {
                    printf("Aucun client actif pour recevoir l'image\n");
                }
            } else {
                printf("Syntaxe incorrecte. Usage: broadcast_image [chemin_image]\n");
            }
        }
        else if (strcmp(cmd_buffer, "quit") == 0) {
            printf("Arrêt du serveur...\n");
            running = 0;
            break;
        }
        else {
            printf("Commande inconnue\n");
        }
    }
    
    return NULL;
}

int main() {
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t client_len = sizeof(client_addr);
    ImageReceiver *current_receiver = NULL;
    char filename[100];
    pthread_t cmd_thread_id;
    
    // Initialiser le tableau des clients
    init_clients();
    
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
    
    // Créer le thread pour les commandes
    if (pthread_create(&cmd_thread_id, NULL, command_thread, NULL) != 0) {
        perror("Erreur lors de la création du thread de commandes");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    // Créer un dossier pour stocker les images reçues s'il n'existe pas
    if (access("received_images", F_OK) != 0) {
        if (mkdir("received_images", 0755) != 0) {
            perror("Impossible de créer le dossier 'received_images'");
        } else {
            printf("Dossier 'received_images' créé\n");
        }
    }
    
    while (running) {
        // Configuration du timeout pour la fonction select
        fd_set read_fds;
        struct timeval tv;
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        tv.tv_sec = 1;  // Vérification toutes les secondes
        tv.tv_usec = 0;
        
        // Vérifier si des données sont disponibles
        int ready = select(sockfd + 1, &read_fds, NULL, NULL, &tv);
        
        // Nettoyer périodiquement les clients inactifs
        static time_t last_cleanup = 0;
        time_t current_time = time(NULL);
        if (current_time - last_cleanup > 60) {  // Nettoyage toutes les minutes
            cleanup_clients();
            last_cleanup = current_time;
        }
        
        // Vérifier le timeout pour l'image en cours
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
        
        if (n <= 0) continue;
        
        // Mettre à jour les informations du client
        update_client(&client_addr, client_len);
        
        if (n <= sizeof(ImageFragmentHeader)) {
            printf("Paquet trop petit reçu, ignoré\n");
            continue;
        }
        
        // Extraction de l'en-tête
        ImageFragmentHeader header;
        memcpy(&header, buffer, sizeof(header));
        
        // Affichage des informations du fragment
        printf("Fragment reçu de %s:%d: ID=%u, Seq=%u/%u, Taille=%u\n", 
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
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
            
            // Générer un nom de fichier unique basé sur l'heure
            sprintf(filename, "received_images/image_%u_%ld.jpg", 
                    current_receiver->image_id, time(NULL));
            
            // Sauvegarder l'image
            save_image(current_receiver, filename);
            
            // Libérer les ressources
            free_image_receiver(current_receiver);
            current_receiver = NULL;
        }
    }
    
    // Attendre la fin du thread de commandes
    pthread_join(cmd_thread_id, NULL);
    
    // Libérer les ressources
    if (current_receiver) {
        free_image_receiver(current_receiver);
    }
    
    close(sockfd);
    printf("Serveur arrêté\n");
    
    return 0;
}