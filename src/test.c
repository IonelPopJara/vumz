#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "audio-cap.h"

// gcc -o test_program test.c audio-cap.c -lm -lpipewire-0.3 -I/usr/include/pipewire-0.3 -I/usr/include/spa-0.2 -D_REENTRANT

int main(int argc, char*argv[]) {
    pthread_t audio_thread;

    struct audio_data audio = {};

    // Create a thread to run the input function
    if (pthread_create(&audio_thread, NULL, input_pipewire, (void*)&audio) != 0) {
        fprintf(stderr, "Error");
        return EXIT_FAILURE;
    }

    printf("Initializing\n");

    while (1) {
        printf("Main thread: \n");
        printf("\t%f -- L\n", audio.left_channel_dbs);
        printf("\t%f -- R\n", audio.right_channel_dbs);
        /*sleep(1);*/
    }

    /*// Main thread running here*/
    /*for (int i = 0; i < 10; i++) {*/
    /*    printf("Main thread running... %d\n", i + 1);*/
    /*    sleep(1);*/
    /*}*/

    if (pthread_join(audio_thread, NULL) != 0) {
        fprintf(stderr, "Error");
        return EXIT_FAILURE;
    }


    printf("Audio finished\n");

    return EXIT_SUCCESS;
}
