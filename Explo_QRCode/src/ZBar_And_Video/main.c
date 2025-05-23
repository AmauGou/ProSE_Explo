#include "Test_2.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <zbar.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

// Variable globale pour le main loop
GMainLoop* loop = NULL;

// Fonction callback pour les erreurs
static void on_error_message(GstBus *bus, GstMessage *message, gpointer user_data) {
    GError *err = NULL;
    gchar *debug;
    gst_message_parse_error(message, &err, &debug);
    g_print("Error: %s\n", err->message);
    g_print("Debug info: %s\n", debug);
    g_error_free(err);
    g_free(debug);
    
    if (loop) g_main_loop_quit(loop);
}

// Fonction callback pour les avertissements
static void on_warning_message(GstBus *bus, GstMessage *message, gpointer user_data) {
    GError *err = NULL;
    gchar *debug;
    gst_message_parse_warning(message, &err, &debug);
    g_print("Warning: %s\n", err->message);
    g_print("Debug info: %s\n", debug);
    g_error_free(err);
    g_free(debug);
}

// Fonction callback pour les changements d'état
static void on_state_changed(GstBus *bus, GstMessage *message, gpointer user_data) {
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(message, &old_state, &new_state, &pending_state);
    if (GST_MESSAGE_SRC(message) == GST_OBJECT(user_data)) {
        g_print("Pipeline state changed from %s to %s\n",
                gst_element_state_get_name(old_state),
                gst_element_state_get_name(new_state));
    }
}

// Callback pour la fin du flux
static void on_eos_message(GstBus *bus, GstMessage *message, gpointer user_data) {
    g_print("End of stream received\n");
    if (loop) g_main_loop_quit(loop);
}

// Callback qui reçoit les frames du pipeline GStreamer
static GstFlowReturn on_new_sample(GstAppSink* appsink, gpointer user_data) {
    g_print("New sample callback triggered!\n");
    GstSample* sample = gst_app_sink_pull_sample(appsink);
    if (!sample) {
        g_print("Failed to pull sample\n");
        return GST_FLOW_ERROR;
    }

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstCaps* caps = gst_sample_get_caps(sample);
    GstStructure* s = gst_caps_get_structure(caps, 0);

    int width, height;
    gst_structure_get_int(s, "width", &width);
    gst_structure_get_int(s, "height", &height);
    g_print("Frame received: %dx%d\n", width, height);

    // Récupérer le format exact
    const gchar* format_str = gst_structure_get_string(s, "format");
    g_print("Format: %s\n", format_str ? format_str : "unknown");

    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        g_print("Failed to map buffer\n");
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    // Le format doit être GRAY8 pour ZBar : Y800
    uint8_t* gray_data = (uint8_t*)malloc(width * height);
    if (!gray_data) {
        g_print("Failed to allocate memory for frame\n");
        gst_buffer_unmap(buffer, &map);
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    // Copie des données gris
    memcpy(gray_data, map.data, width * height);

    // Analyse QR
    g_print("Analyzing frame for QR codes...\n");
    int qr_found = decode_qr_from_buffer(gray_data, width, height);
    if (qr_found == 0) {
        g_print("No QR code detected in this frame.\n");
    } else {
        g_print("QR code found! Value: %d\n", qr_found);
    }

    free(gray_data);
    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);

    return GST_FLOW_OK;
}

// Fonction pour tester si le périphérique vidéo existe
static gboolean check_video_device(const char* device) {
    FILE* fd = fopen(device, "r");
    if (fd == NULL) {
        g_print("Error: Video device %s not found or cannot be accessed\n", device);
        return FALSE;
    }
    fclose(fd);
    g_print("Video device %s found and accessible\n", device);
    return TRUE;
}

// Fonction pour lister tous les périphériques vidéo disponibles
static void list_video_devices() {
    g_print("Searching for video devices...\n");
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ls -l /dev/video* 2>/dev/null || echo 'No video devices found'");
    system(cmd);
    
    g_print("\nChecking for Raspberry Pi camera module...\n");
    if (access("/dev/vchiq", F_OK) != -1) {
        g_print("Raspberry Pi camera interface detected (/dev/vchiq exists)\n");
    } else {
        g_print("Raspberry Pi camera interface not detected (/dev/vchiq missing)\n");
    }
}

// Fonction pour tester simplement le pipeline
static void test_pipeline() {
    GError* error = NULL;
    g_print("Testing simple pipeline with fakesink...\n");
    GstElement* test_pipeline = gst_parse_launch(
        "v4l2src device=/dev/video0 ! fakesink", &error);
    
    if (!test_pipeline) {
        g_print("Failed to create test pipeline: %s\n", error->message);
        g_error_free(error);
        return;
    }
    
    GstStateChangeReturn ret = gst_element_set_state(test_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_print("Failed to set test pipeline to playing state\n");
    } else {
        g_print("Test pipeline started successfully\n");
        sleep(1);  // Attendre 1 seconde
    }
    
    gst_element_set_state(test_pipeline, GST_STATE_NULL);
    gst_object_unref(test_pipeline);
}

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);
    
    // Lister tous les périphériques vidéo disponibles
    // list_video_devices();
    
    // Tester un pipeline simple
    // test_pipeline();
    
    const char* video_device = "/dev/video0";
    // Vérifier si le périphérique vidéo existe
    if (!check_video_device(video_device)) {
        g_print("Please check your camera connection or permissions\n");
        return -1;
    }

    // Déterminer quel type de pipeline utiliser
    const char* pipeline_desc;
    gboolean use_rpi_cam = FALSE;
    
    // Vérifier si c'est une caméra Raspberry Pi
    if (access("/dev/vchiq", F_OK) != -1) {
        g_print("Using Raspberry Pi camera specific pipeline\n");
        use_rpi_cam = TRUE;
        // Pipeline pour caméra Raspberry Pi
        pipeline_desc =
            "libcamerasrc ! tee name=t "
            // Branche 1 : pour le streaming TCP
            "t. ! queue max-size-buffers=0 max-size-bytes=0 max-size-time=1000000000 ! "
            "video/x-raw, width=640, height=480, framerate=10/1, format=I420 ! "
            "videoconvert ! "
            "x264enc tune=zerolatency byte-stream=true key-int-max=30 speed-preset=ultrafast "
            "bitrate=1000 ! "
            "video/x-h264, stream-format=byte-stream, alignment=au, profile=baseline ! "
            "tcpserversink host=0.0.0.0 port=4000 "
            // Branche 2 : pour le QR code
            "t. ! queue ! "
            "videoconvert ! "
            "videoscale ! "
            "video/x-raw, width=640, height=480, format=GRAY8 ! "
            "appsink name=appsink sync=false";
    } else {
        // Pipeline standard pour webcam USB
        g_print("Using standard webcam pipeline\n");
        pipeline_desc =
            "libcamerasrc ! tee name=t "
            // Branche 1 : pour le streaming TCP
            "t. ! queue max-size-buffers=0 max-size-bytes=0 max-size-time=1000000000 ! "
            "video/x-raw, width=640, height=480, framerate=10/1, format=I420 ! "
            "videoconvert ! "
            "x264enc tune=zerolatency byte-stream=true key-int-max=30 speed-preset=ultrafast "
            "bitrate=1000 ! "
            "video/x-h264, stream-format=byte-stream, alignment=au, profile=baseline ! "
            "tcpserversink host=0.0.0.0 port=4000 "
            // Branche 2 : pour le QR code
            "t. ! queue ! "
            "videoconvert ! "
            "videoscale ! "
            "video/x-raw, width=640, height=480, format=GRAY8 ! "
            "appsink name=appsink sync=false";
    }

    g_print("Pipeline description: %s\n", pipeline_desc);

    // Tentative alternative si premier pipeline échoue
    GError* error = NULL;
    GstElement* pipeline = gst_parse_launch(pipeline_desc, &error);
    
    if (!pipeline) {
        g_print("Failed to create pipeline: %s\n", error->message);
        g_error_free(error);
    }

    /*
    g_print("Is pipeline a GstPipeline? %d\n", GST_IS_PIPELINE(pipeline));
    g_print("Is pipeline a GstBin? %d\n", GST_IS_BIN(pipeline));
    */
   
    // Récupérer et configurer l'appsink
    GstElement* appsink = gst_bin_get_by_name(GST_BIN(pipeline), "appsink");
    if (!appsink) {
        g_print("Failed to get appsink element\n");
        gst_object_unref(pipeline);
        return -1;
    }

    /*
    // Configuration du bus pour les messages d'erreur
    GstBus* bus = gst_element_get_bus(pipeline);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(bus, "message::error", G_CALLBACK(on_error_message), NULL);
    g_signal_connect(bus, "message::warning", G_CALLBACK(on_warning_message), NULL);
    g_signal_connect(bus, "message::state-changed", G_CALLBACK(on_state_changed), pipeline);
    g_signal_connect(bus, "message::eos", G_CALLBACK(on_eos_message), NULL);
    gst_object_unref(bus);
    */

    // Configuration de l'appsink avec callbacks
    GstAppSinkCallbacks callbacks = {NULL};
    callbacks.new_sample = on_new_sample;
    gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, NULL, NULL);
    gst_app_sink_set_emit_signals(GST_APP_SINK(appsink), TRUE);
    gst_app_sink_set_max_buffers(GST_APP_SINK(appsink), 1);
    gst_app_sink_set_drop(GST_APP_SINK(appsink), TRUE);
    gst_object_unref(appsink);

    // Démarrer le pipeline
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_print("Failed to set pipeline to playing state\n");
        gst_object_unref(pipeline);
        return -1;
    }

    // Vérifier si le pipeline atteint l'état PLAYING
    GstState state;
    ret = gst_element_get_state(pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
    if (ret != GST_STATE_CHANGE_SUCCESS) {
        g_print("Failed to reach playing state: %d\n", ret);
    } else {
        g_print("Pipeline is now in state: %s\n", gst_element_state_get_name(state));
    }

    g_print("Video capture started, QR code analysis in progress...\n");
    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    // Nettoyage
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    // Ajouter un mutex
    if (loop) g_main_loop_unref(loop);

    return 0;
}