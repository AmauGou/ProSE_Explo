#include "Test_2.h"

static const char* image_path = "/tmp/cam_frame.jpg";

int main() {
    char cmd[256];

    while (1) {
        // 1. Capturer une image avec la caméra
        snprintf(cmd, sizeof(cmd), "libcamera-still -n -o %s --width 640 --height 480 --timeout 100", image_path);
        system(cmd);

        // 2. Analyser l'image
        printf("Analyse d’une nouvelle frame...\n");
        decode_qr(image_path);

        // 3. Attendre un petit peu avant la prochaine image
        usleep(500 * 1000);  // 500 ms
    }
    printf("QR Code trouvé\n");
    return 0;
}