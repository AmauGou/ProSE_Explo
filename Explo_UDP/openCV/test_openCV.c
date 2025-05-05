#include <opencv2/opencv.h>
#include <stdio.h>

int main()
{
    CvCapture* cap = cvCaptureFromCAM(0); // Ouvre la caméra par défaut
    if (!cap) {
        printf("Erreur : Impossible d'ouvrir la caméra !\n");
        return -1;
    }

    cvNamedWindow("frame", CV_WINDOW_AUTOSIZE);
    
    while (1) {
        IplImage* frame = cvQueryFrame(cap);
        if (!frame) {
            printf("Erreur : Impossible de capturer une image !\n");
            break;
        }

        cvShowImage("frame", frame);
        
        if (cvWaitKey(30) >= 0) {
            break;
        }
    }

    cvReleaseCapture(&cap);
    cvDestroyWindow("frame");
    
    return 0;
}
