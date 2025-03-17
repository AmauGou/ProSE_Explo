CC = gcc
CFLAGS = -Wall

all: udp_client_photo

client: client.c
	$(CC) $(CFLAGS) udp_client_photo.c -o udp_client_photo

clean:
	rm -f udp_client_photo received_image.jpg
