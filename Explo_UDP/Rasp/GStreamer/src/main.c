

#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#define WIDTH 640
#define HEIGHT 480
#define FRAMERATE 30
#define DEVICE "/dev/video0"
#define PHONE_IP "192.168.1.131"  // Remplace par l'IP de ton téléphone
#define PORT 5000

int main(int argc, char *argv[]) {
    GstElement *pipeline = NULL;
    GstBus *bus = NULL;
    GstMessage *msg = NULL;
    GError *error = NULL;
    gchar *pipeline_str = NULL;

    // Initialiser GStreamer
    gst_init(&argc, &argv);

    /*
    // Construction du pipeline GStreamer pour capturer, afficher localement, et diffuser une vidéo VP8 via UDP
    pipeline_str = g_strdup_printf(
        "v4l2src device=%s ! "
        "videoconvert ! "
        "videoscale ! "
        "video/x-raw,format=I420,width=%d,height=%d,framerate=%d/1 ! "  // Changer le format en I420
        "tee name=t ! "
        "queue ! autovideosink ! "
        "t. ! queue ! vp8enc deadline=1 ! rtpvp8pay pt=96 ! udpsink host=%s port=%d",
        DEVICE, WIDTH, HEIGHT, FRAMERATE, PHONE_IP, PORT);
    */
    
    pipeline_str = g_strdup_printf(
    "libcamerasrc ! "
    //"videoconvert ! "
    "videoscale ! "
    "video/x-raw,width=%s,height=%d,framerate=%d/1 ! "
    "autovideosink",
    DEVICE, WIDTH, HEIGHT, FRAMERATE);

    // Construction du pipeline à partir de la chaîne formatée
    pipeline = gst_parse_launch(pipeline_str, &error);

    g_free(pipeline_str);  // Libérer la chaîne formatée

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
