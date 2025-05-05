#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <errno.h>
#include <sys/mman.h>


#define WIDTH 640
#define HEIGHT 480
#define NUM_BUFFERS 4

// Nous avons des problèmes avec le format H264 alors que ce serait le plus 
// optimisé pour notre cas

// Structure pour les buffers en mémoire MMap
struct buffer {
    void *start;
    size_t length;
};

// Fonction pour ouvrir le périphérique vidéo
int open_video_device(const char *dev_name) {
    int fd = open(dev_name, O_RDWR);
    if (fd == -1) {
        perror("Erreur d'ouverture du périphérique");
        return -1;
    }
    return fd;
}

// Fonction pour configurer le format vidéo 
int set_video_format(int fd) {
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("Erreur de configuration du format");
        return -1;
    }
    return 0;
}

// Fonction pour allouer les buffers en mémoire MMap
int allocate_buffers(int fd, struct buffer *buffers) {
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = NUM_BUFFERS;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        perror("Erreur de demande de buffers");
        return -1;
    }

    for (int i = 0; i < NUM_BUFFERS; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            perror("Erreur de requête de buffer");
            return -1;
        }

        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (buffers[i].start == MAP_FAILED) {
            perror("Erreur de mappage de buffer");
            return -1;
        }
    }

    return 0;
}

// Fonction pour mettre les buffers en file d'attente
int enqueue_buffers(int fd) {
    for (int i = 0; i < NUM_BUFFERS; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("Erreur d'enfilement de buffer");
            return -1;
        }
    }
    return 0;
}

// Fonction pour démarrer la capture vidéo
int start_capture(int fd) {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        perror("Erreur de démarrage du flux vidéo");
        return -1;
    }
    return 0;
}

// Fonction pour capturer et envoyer les frames à FFmpeg via un pipe
int capture_and_stream(int fd, struct buffer *buffers, FILE *ffmpeg_pipe) {
    for (int i = 0; i < 100; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
            perror("Erreur de récupération de buffer");
            return -1;
        }

        // Debugging de la taille de la frame
        printf("Frame %d: %d bytes used\n", i + 1, buf.bytesused);

        // Vérification de la validité des données
        if (buf.bytesused > 0) {
            fwrite(buffers[buf.index].start, 1, buf.bytesused, ffmpeg_pipe);
            fflush(ffmpeg_pipe);  // S'assurer que les données sont envoyées au pipe
        } else {
            printf("Aucune donnée valide pour la frame %d\n", i + 1);
        }

        // Remettre le buffer en file d'attente
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("Erreur de remise en file d'attente du buffer");
            return -1;
        }
    }
    return 0;
}

// Fonction pour arrêter la capture vidéo
int stop_capture(int fd) {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
        perror("Erreur d'arrêt du flux vidéo");
        return -1;
    }
    return 0;
}

// Fonction pour libérer les ressources
void free_buffers(struct buffer *buffers) {
    for (int i = 0; i < NUM_BUFFERS; i++) {
        munmap(buffers[i].start, buffers[i].length);
    }
}

int main() {
    int fd = open_video_device("/dev/video0");
    if (fd == -1) return 1;

    if (set_video_format(fd) == -1) {
        perror("Erreur de convertisseur du format vidéo\n");
        close(fd);
        return 1;
    }

    struct buffer buffers[NUM_BUFFERS];
    if (allocate_buffers(fd, buffers) == -1) {
        perror("Erreur d'allocations des buffers'\n");
        close(fd);
        return 1;
    }

    if (enqueue_buffers(fd) == -1) {
        perror("Erreur dans la queue des buffers\n");
        close(fd);
        return 1;
    }

    if (start_capture(fd) == -1) {
        perror("Erreur de capture de la vidéo\n");
        close(fd);
        return 1;
    }

    // Ouvrir le pipe FFmpeg pour l'encodage
    FILE *ffmpeg_pipe = popen("ffmpeg -f rawvideo -pix_fmt yuyv422 -s 640x480 -r 25 -i pipe: -c:v libx264 -f mpegts udp://192.168.1.1:12345", "w");
    if (!ffmpeg_pipe) {
        perror("Erreur d'ouverture du pipe FFmpeg");
        close(fd);
        return 1;
    }

    if (capture_and_stream(fd, buffers, ffmpeg_pipe) == -1) {
        fclose(ffmpeg_pipe);
        close(fd);
        return 1;
    }

    if (stop_capture(fd) == -1) {
        fclose(ffmpeg_pipe);
        close(fd);
        return 1;
    }

    // Libérer les ressources
    free_buffers(buffers);
    fclose(ffmpeg_pipe);
    close(fd);
    return 0;
}
