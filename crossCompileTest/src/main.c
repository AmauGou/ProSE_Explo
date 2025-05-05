#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>

#define WIDTH 640
#define HEIGHT 480
#define FRAMERATE 15
#define DEVICE "/dev/video0"

int main(int argc, char *argv[]) {
    GstElement *pipeline;
    GstBus *bus;
    GstMessage *msg;
    GError *error = NULL;

    // Initialiser GStreamer
    gst_init(&argc, &argv);

    // Construire dynamiquement la chaîne de pipeline pour affichage sur l'écran
    printf("Construction du pipeline...\n");
    gchar *pipeline_desc = g_strdup_printf(
        "v4l2src device=%s ! video/x-raw, width=%d, height=%d, framerate=%d/1 ! "
        "videoconvert ! autovideosink",
        DEVICE, WIDTH, HEIGHT, FRAMERATE);

    g_print("Pipeline : %s\n", pipeline_desc);

    pipeline = gst_parse_launch(pipeline_desc, &error);
    g_free(pipeline_desc); // Libérer la mémoire allouée

    if (!pipeline) {
        g_printerr("Erreur lors de la création du pipeline: %s\n", error->message);
        g_clear_error(&error);
        return -1;
    }

    // Lancer le pipeline
    printf("Lancement du pipeline...\n");
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
