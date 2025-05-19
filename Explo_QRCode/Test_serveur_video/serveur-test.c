#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <SDL2/SDL.h>

#define PORT 8888
#define BUFFER_SIZE 4096

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Fonction pour trouver un JPEG complet dans le buffer
int find_jpeg_frame(unsigned char *buf, int len, int *start, int *end) {
    int i;
    *start = -1;
    *end = -1;
    for (i = 0; i < len - 1; i++) {
        if (*start == -1 && buf[i] == 0xFF && buf[i + 1] == 0xD8) {
            *start = i;
        }
        if (*start != -1 && buf[i] == 0xFF && buf[i + 1] == 0xD9) {
            *end = i + 1;
            return 1;
        }
    }
    return 0;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);
    unsigned char buffer[BUFFER_SIZE];
    unsigned char frame_buffer[10 * 1024 * 1024]; // 10MB max
    int frame_buf_len = 0;

    // Initialisation SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Erreur SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Flux MJPEG",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          640, 480, 0);
    if (!window) {
        fprintf(stderr, "Erreur SDL_CreateWindow: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        fprintf(stderr, "Erreur SDL_CreateRenderer: %s\n", SDL_GetError());
        return 1;
    }

    // Socket serveur
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 1);
    printf("En attente d'un client sur le port %d...\n", PORT);
    client_fd = accept(server_fd, (struct sockaddr *)&address, &addr_len);
    printf("Client connecté.\n");

    int start = 0, end = 0;

    while (1) {
        ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE);
        if (bytes_read <= 0) break;

        if (frame_buf_len + bytes_read > sizeof(frame_buffer)) {
            fprintf(stderr, "Dépassement de buffer\n");
            break;
        }

        memcpy(frame_buffer + frame_buf_len, buffer, bytes_read);
        frame_buf_len += bytes_read;

        while (find_jpeg_frame(frame_buffer, frame_buf_len, &start, &end)) {
            int frame_len = end - start + 1;

            int w, h, comp;
            unsigned char *img = stbi_load_from_memory(frame_buffer + start, frame_len, &w, &h, &comp, 3);
            if (!img) {
                fprintf(stderr, "Erreur de décodage JPEG\n");
                memmove(frame_buffer, frame_buffer + end + 1, frame_buf_len - (end + 1));
                frame_buf_len -= (end + 1);
                continue;
            }

            SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(img, w, h, 24, w * 3,
                                                            0xFF0000, 0x00FF00, 0x0000FF, 0);
            if (!surface) {
                fprintf(stderr, "Erreur SDL_CreateRGBSurfaceFrom: %s\n", SDL_GetError());
                stbi_image_free(img);
                break;
            }

            SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);

            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);

            SDL_DestroyTexture(texture);
            stbi_image_free(img);

            // Retirer la frame traitée
            memmove(frame_buffer, frame_buffer + end + 1, frame_buf_len - (end + 1));
            frame_buf_len -= (end + 1);

            // Gestion d'événements SDL
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    close(client_fd);
                    close(server_fd);
                    SDL_DestroyRenderer(renderer);
                    SDL_DestroyWindow(window);
                    SDL_Quit();
                    return 0;
                }
            }
        }
    }

    printf("Connexion fermée\n");
    close(client_fd);
    close(server_fd);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
