#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <zbar.h>
#include "Decode_QR.h"

#define DETECTION_TIME 3.0

static const char* image_path = "/tmp/cam_frame.jpg";


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

// Fonction pour décoder les QR codes à partir d'une image en niveaux de gris
int decode_qr_from_buffer(uint8_t* gray_data, int width, int height) {
    // Création d'un scanner ZBar pour la détection de QR codes
    zbar_image_scanner_t* scanner = zbar_image_scanner_create();
    
    // Activation de la détection pour tous les types de symboles (ici, QR codes inclus)
    zbar_image_scanner_set_config(scanner, 0, ZBAR_CFG_ENABLE, 1);

    // Création d'une image ZBar
    zbar_image_t* image = zbar_image_create();

    // Définition du format de l'image : 'Y800' signifie image en niveaux de gris (8 bits par pixel)
    zbar_image_set_format(image, zbar_fourcc('Y','8','0','0'));

    // Définition de la taille de l'image
    zbar_image_set_size(image, width, height);

    // Passage des données d'image à ZBar (le buffer est géré par l'appelant, pas libéré ici)
    zbar_image_set_data(image, gray_data, width * height, NULL);

    // Lancement du scan de l'image à la recherche de codes
    int n = zbar_scan_image(scanner, image);

    // Si au moins un symbole est détecté
    if (n > 0) {
        // Récupération du premier symbole trouvé
        const zbar_symbol_t* symbol = zbar_image_first_symbol(image);

        // Parcours de tous les symboles détectés
        for (; symbol; symbol = zbar_symbol_next(symbol)) {
            // Extraction des données du symbole (chaîne de caractères contenue dans le QR code)
            const char* data = zbar_symbol_get_data(symbol);
            printf("QR Code détecté : %s\n", data);

            // Identification du type d'entité représentée par le QR code
            EntityType current_type = get_entity_type(data);
            time_t current_time = time(NULL);  // Heure actuelle

            // Si une entité valide est détectée
            if (current_type != NONE && current_type != UNKNOWN) {
                // Vérifie s'il s'agit d'une transition rapide entre deux types d'entités différents
                if (last_entity_type != NONE &&
                    current_type != last_entity_type &&
                    difftime(current_time, last_detection_time) <= DETECTION_TIME) {
                    // Cas particulier où deux entités différentes sont détectées en peu de temps
                    printf("Cas spécial : ALLY_TARGET détecté\n");
                }

                // Mise à jour du dernier type détecté et du temps de détection
                last_entity_type = current_type;
                last_detection_time = current_time;
            }
        }
    }

    // Libération des ressources ZBar utilisées
    zbar_image_destroy(image);
    zbar_image_scanner_destroy(scanner);

    // Retourne le nombre de symboles détectés
    return n;
}

