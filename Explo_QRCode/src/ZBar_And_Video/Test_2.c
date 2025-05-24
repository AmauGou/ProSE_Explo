#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include "stb_image.h"
#include <zbar.h>

#define STB_IMAGE_IMPLEMENTATION
#define DETECTION_TIME 3.0

static const char* image_path = "/tmp/cam_frame.jpg";

/*
int decode_qr(const char* path) { 
    int width, height, channels;

    // Charger l'image en niveaux de gris (1 canal)
    unsigned char* image = stbi_load(image_path, &width, &height, &channels, 1);
    if (!image) {
        fprintf(stderr, "Erreur : impossible de charger l'image %s\n", image_path);
        return 1;
    }

    // Initialiser le scanner ZBar
    zbar_image_scanner_t* scanner = zbar_image_scanner_create();
    zbar_image_scanner_set_config(scanner, 0, ZBAR_CFG_ENABLE, 1);

    // Créer une image ZBar
    zbar_image_t* zimg = zbar_image_create();
    zbar_image_set_format(zimg, zbar_fourcc('Y','8','0','0'));
    zbar_image_set_size(zimg, width, height);
    zbar_image_set_data(zimg, image, width * height, NULL);  // on gère nous-mêmes le free

    // Scanner
    int n = zbar_scan_image(scanner, zimg);
    if (n == 0) {
    printf("Aucun QR Code détecté.\n");
    remove(path);
    return 1;
    } else {
        const zbar_symbol_t* symbol = zbar_image_first_symbol(zimg);
        for (; symbol; symbol = zbar_symbol_next(symbol)) {
            printf("QR Code détecté : %s\n", zbar_symbol_get_data(symbol));
        }
        return 0;  // QR trouvé
    }

    // Nettoyage
    zbar_image_destroy(zimg);
    zbar_image_scanner_destroy(scanner);
    stbi_image_free(image);

    return 0;

}
*/

// Simuler une base de données
const char* allies[] = {"http://192.168.8.205", "192.168.8.222", "192.168.8.333"};
const char* enemies[] = {"http://192.168.8.1", "192.168.8.2", "192.168.8.3"};
const int num_allies = 3;
const int num_enemies = 3;

typedef enum { 
    NONE, 
    ALLY, 
    TARGET,
    UNKNOWN,
 } 
    EntityType;

EntityType last_entity_type = NONE;
time_t last_detection_time = 0;

EntityType get_entity_type(const char* id) {
    for (int i = 0; i < num_allies; ++i) {
        if (strcmp(id, allies[i]) == 0) {
            printf("ALLY detected : %s\n",id);
            return ALLY;
        }   
    }

    for (int i = 0; i < num_enemies; ++i) {
        if (strcmp(id, enemies[i]) == 0) {
            printf("TARGET detected : %s\n",id);
            return TARGET;
        } 
    }
    return UNKNOWN;
}

int decode_qr_from_buffer(uint8_t* gray_data, int width, int height) {
    zbar_image_scanner_t* scanner = zbar_image_scanner_create();
    zbar_image_scanner_set_config(scanner, 0, ZBAR_CFG_ENABLE, 1);

    zbar_image_t* image = zbar_image_create();
    zbar_image_set_format(image, zbar_fourcc('Y','8','0','0'));
    zbar_image_set_size(image, width, height);
    zbar_image_set_data(image, gray_data, width * height, NULL);  // Memory owned by caller

    int n = zbar_scan_image(scanner, image);

    if (n > 0) {
        const zbar_symbol_t* symbol = zbar_image_first_symbol(image);
        for (; symbol; symbol = zbar_symbol_next(symbol)) {
            const char* data = zbar_symbol_get_data(symbol);
            printf("QR Code détecté : %s\n", data);

            EntityType current_type = get_entity_type(data);
            time_t current_time = time(NULL);

            if (current_type != NONE && current_type != UNKNOWN) {
                if (last_entity_type != NONE &&
                    current_type != last_entity_type &&
                    difftime(current_time, last_detection_time) <= DETECTION_TIME) {
                    printf("Cas spécial : ALLY_TARGET détecté\n");
                }

                last_entity_type = current_type;
                last_detection_time = current_time;
            }
        }
    }

    zbar_image_destroy(image);
    zbar_image_scanner_destroy(scanner);

    return n;
}

