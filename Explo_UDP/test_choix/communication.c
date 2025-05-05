#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT_TCP 8080
#define PORT_UDP 8081
#define BUFFER_SIZE 1024

void communicationUDP();
void communicationTCP();

// Déclaration d'un tableau de pointeurs de fonctions 
void (*choixCommunication[])() = {communicationUDP, communicationTCP};


void communicationUDP() {
    int sockfd;
    struct sockaddr_in serverAddr, clientAddr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_size;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Erreur de socket UDP");
        exit(EXIT_FAILURE);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT_UDP);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Erreur de bind UDP");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Serveur UDP en attente de messages...\n");
    addr_size = sizeof(clientAddr);
    recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&clientAddr, &addr_size);
    printf("Message reçu en UDP: %s\n", buffer);

    close(sockfd);
}

void communicationTCP() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("Erreur de socket TCP");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT_TCP);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Erreur de bind TCP");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 3) < 0) {
        perror("Erreur d'écoute TCP");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Serveur TCP en attente de connexion...\n");
    new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
    if (new_socket < 0) {
        perror("Erreur d'acceptation TCP");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    read(new_socket, buffer, BUFFER_SIZE);
    printf("Message reçu en TCP: %s\n", buffer);

    close(new_socket);
    close(server_fd);
}

int main() {
    int choix;

    printf("Choisissez le mode de communication :\n1. UDP\n2. TCP\n");
    scanf("%d", &choix);

    if (choix == 1 || choix == 2) {
        (*choixCommunication[choix - 1])();
    } else {
        printf("Choix invalide.\n");
    }
    
    return 0;
}
