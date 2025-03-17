CC = gcc
CFLAGS = -Wall

all: udp_serveur_photo udp_client_photo

server: server.c
	$(CC) $(CFLAGS) udp_serveur_photo.c -o udp_serveur_photo

client: client.c
	$(CC) $(CFLAGS) udp_client_photo.c -o udp_client_photo

clean:
	rm -f udp_serveur_photo udp_client_photo received_image.jpg
