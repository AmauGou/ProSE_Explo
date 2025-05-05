#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#define WIDTH 1280
#define HEIGHT 720
#define FRAMERATE 30
#define DEVICE "/dev/video0"
#define PHONE_IP "192.168.1.131"  // Remplace par l'IP de ton téléphone
#define PORT 5000

int main(int argc, char *argv[]) {
    GstElement *pipeline;
    GstBus *bus;
    GstMessage *msg;
    GError *error = NULL;

    // Initialiser GStreamer
    gst_init(&argc, &argv);

    // Construction du pipeline GStreamer pour capturer, afficher localement, et diffuser une vidéo VP8 via UDP
    pipeline = gst_parse_launch(
        // v4l2src : Capture la vidéo depuis un périphérique V4L2 (ex: webcam)
        "v4l2src device=" DEVICE " ! "

        // videoconvert : Convertit le format vidéo brut pour qu’il soit compatible avec les traitements suivants
        "videoconvert ! "

        // videoscale : Permet de redimensionner la vidéo si nécessaire
        "videoscale ! "

        // Capability filter : Définit la résolution et le framerate de sortie
        "video/x-raw,format=RGB, width=" GST_STR(WIDTH) ",height=" GST_STR(HEIGHT) ",framerate=" GST_STR(FRAMERATE) "/1 ! "

        // tee : Duplique le flux vidéo en deux branches (local display + réseau)
        "tee name=t ! "

        // Branche 1 : affichage local
        "queue ! autovideosink "

        // Branche 2 : encodage VP8 + transmission réseau
        "t. ! queue ! vp8enc deadline=1 ! rtpvp8pay pt=96 ! udpsink host=" PHONE_IP " port=" GST_STR(PORT),
        &error);

    if (!pipeline) {
        g_printerr("Erreur lors de la création du pipeline: %s\n", error->message);
        g_clear_error(&error);
        return -1;
    }

    // Lancer le pipeline
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    // Attendre jusqu'à une erreur ou un arrêt du pipeline
    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    if (msg != NULL) {
        GError *err;
        gchar *debug_info;

        gst_message_parse_error(msg, &err, &debug_info);
        g_printerr("Erreur GStreamer: %s\n", err->message);
        g_error_free(err);
        g_free(debug_info);
        gst_message_unref(msg);
    }

    // Nettoyage
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;
}
