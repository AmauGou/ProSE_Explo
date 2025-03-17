#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <libavformat/avformat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

#define PORT 12345
#define SERVER_IP "172.14.1.16"
#define MAX_UDP_SIZE 8192  // Taille maximale sécurisée pour UDP

// Structure d'en-tête pour les fragments
typedef struct {
    int frame_id;      // ID de la frame
    int fragment_id;   // ID du fragment dans la frame
    int fragment_count; // Nombre total de fragments pour cette frame
    int data_size;     // Taille des données dans ce fragment
    int original_size; // Taille originale de la frame complète
} PacketHeader;

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    AVFormatContext *format_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    const AVCodec *codec = NULL;
    AVPacket *packet;
    int frame_count = 0;

    printf("Démarrage du serveur UDP vidéo...\n"); fflush(stdout);

    // Vérifier si le fichier existe
    if (access("timelapse.mp4", F_OK) != 0) {
        fprintf(stderr, "Le fichier timelapse.mp4 n'existe pas ou n'est pas accessible.\n");
        exit(EXIT_FAILURE);
    }
    printf("Fichier timelapse.mp4 trouvé.\n"); fflush(stdout);

    // Création du socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Erreur de création du socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket UDP créé avec succès.\n"); fflush(stdout);

    // Configuration de l'adresse du serveur
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    printf("Adresse du serveur configurée: %s:%d\n", SERVER_IP, PORT); fflush(stdout);

    // Ouverture du fichier vidéo
    printf("Tentative d'ouverture du fichier vidéo...\n"); fflush(stdout);
    if (avformat_open_input(&format_ctx, "timelapse.mp4", NULL, NULL) != 0) {
        fprintf(stderr, "Erreur d'ouverture du fichier vidéo.\n");
        exit(EXIT_FAILURE);
    }
    printf("Fichier vidéo ouvert avec succès.\n"); fflush(stdout);

    // Récupération des informations du flux
    printf("Récupération des informations du flux...\n"); fflush(stdout);
    if (avformat_find_stream_info(format_ctx, NULL) < 0) {
        fprintf(stderr, "Impossible de trouver les informations du flux.\n");
        exit(EXIT_FAILURE);
    }
    printf("Informations du flux récupérées.\n"); fflush(stdout);

    // Recherche du flux vidéo
    int video_stream_index = -1;
    for (int i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }

    if (video_stream_index == -1) {
        fprintf(stderr, "Aucun flux vidéo trouvé.\n");
        exit(EXIT_FAILURE);
    }
    printf("Flux vidéo trouvé à l'index: %d\n", video_stream_index); fflush(stdout);

    // Allocation du contexte codec
    codec_ctx = avcodec_alloc_context3(NULL);
    if (!codec_ctx) {
        fprintf(stderr, "Erreur d'allocation du contexte codec.\n");
        exit(EXIT_FAILURE);
    }
    printf("Contexte codec alloué.\n"); fflush(stdout);

    // Copie des paramètres du codec
    avcodec_parameters_to_context(codec_ctx, format_ctx->streams[video_stream_index]->codecpar);
    
    // Recherche du codec
    codec = avcodec_find_decoder(codec_ctx->codec_id);
    if (!codec) {
        fprintf(stderr, "Codec vidéo introuvable.\n");
        exit(EXIT_FAILURE);
    }
    printf("Codec trouvé: %s\n", codec->name); fflush(stdout);

    // Ouverture du codec
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Erreur lors de l'ouverture du codec.\n");
        exit(EXIT_FAILURE);
    }
    printf("Codec ouvert avec succès.\n"); fflush(stdout);

    // Allocation du paquet
    packet = av_packet_alloc();
    if (!packet) {
        fprintf(stderr, "Erreur d'allocation du paquet.\n");
        exit(EXIT_FAILURE);
    }
    printf("Paquet alloué.\n"); fflush(stdout);

    // Allouer mémoire pour le buffer fragmenté
    unsigned char *fragment_buffer = (unsigned char *)malloc(MAX_UDP_SIZE);
    if (!fragment_buffer) {
        fprintf(stderr, "Erreur d'allocation du buffer de fragment.\n");
        exit(EXIT_FAILURE);
    }

    // Lecture et envoi des frames
    printf("Début de la lecture des frames...\n"); fflush(stdout);
    while (av_read_frame(format_ctx, packet) >= 0) {
        if (packet->stream_index == video_stream_index) {
            frame_count++;
            printf("Frame #%d: taille = %d octets\n", frame_count, packet->size); fflush(stdout);

            // Calculer le nombre de fragments nécessaires
            int data_size = packet->size;
            int max_data_per_packet = MAX_UDP_SIZE - sizeof(PacketHeader);
            int num_fragments = (data_size + max_data_per_packet - 1) / max_data_per_packet;
            
            printf("Fragmentation en %d parties...\n", num_fragments); fflush(stdout);
            
            // Envoyer chaque fragment
            for (int i = 0; i < num_fragments; i++) {
                // Préparer l'en-tête
                PacketHeader header;
                header.frame_id = frame_count;
                header.fragment_id = i;
                header.fragment_count = num_fragments;
                header.original_size = data_size;
                
                // Calculer la taille de ce fragment
                int offset = i * max_data_per_packet;
                int fragment_size = (i == num_fragments - 1) ? 
                                    (data_size - offset) : max_data_per_packet;
                header.data_size = fragment_size;
                
                // Copier l'en-tête dans le buffer
                memcpy(fragment_buffer, &header, sizeof(PacketHeader));
                
                // Copier les données dans le buffer
                memcpy(fragment_buffer + sizeof(PacketHeader), 
                       packet->data + offset, fragment_size);
                
                // Taille totale du paquet à envoyer
                int total_size = sizeof(PacketHeader) + fragment_size;
                
                // Envoyer le fragment
                if (sendto(sockfd, fragment_buffer, total_size, 0, 
                         (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                    perror("Erreur d'envoi du fragment UDP");
                    break;
                }
                
                printf("Fragment %d/%d envoyé: %d octets\n", 
                       i+1, num_fragments, fragment_size); fflush(stdout);
                
                // Délai entre les fragments
                usleep(5000); // 5ms
            }
            
            printf("Frame #%d envoyée complètement\n", frame_count); fflush(stdout);
            // Délai entre les frames
            usleep(10000); // 10ms
        }

        // Libération du paquet
        av_packet_unref(packet);
    }

    printf("Fin de la lecture des frames. Total: %d frames\n", frame_count); fflush(stdout);

    // Libération des ressources
    printf("Libération des ressources...\n"); fflush(stdout);
    free(fragment_buffer);
    close(sockfd);
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);
    printf("Ressources libérées. Programme terminé.\n"); fflush(stdout);

    return 0;
}