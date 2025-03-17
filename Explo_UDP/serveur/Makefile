CC = gcc
CFLAGS = -Wall

all: udp_serveur_photo 

server: server.c
	$(CC) $(CFLAGS) udp_serveur_photo.c -o udp_serveur_photo

clean:
	rm -f udp_serveur_photo  received_image.jpg
