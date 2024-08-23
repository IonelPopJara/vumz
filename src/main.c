/*
 * VUMZ (VU Meter visualiZer)
 */

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <locale.h>
#include <pthread.h>
#include <string.h>
#include "audio-cap.h"
#include "audio-out.h"

#define CLAMP(val, min, max) (val < min ? min : (val > max ? max : val))

float smooth_factor = 1.0f;

void print_help();

int main(int argc, char **argv)
{
    pthread_t audio_thread;
    struct audio_data audio = {};

    // Create a thread to run the input function
    if (pthread_create(&audio_thread, NULL, input_pipewire, (void*)&audio) != 0) {
        fprintf(stderr, "Error");
        return EXIT_FAILURE;
    }

    bool debug_mode = false;
    bool screensaver_mode = false;

    // Check command line arguments
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-D") == 0 || strcmp(argv[i], "--debug") == 0)
        {
            debug_mode = true;
        }
        else if (strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--screensaver") == 0)
        {
            screensaver_mode = true;
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            print_help();
            exit(EXIT_SUCCESS);
        }
        else {
            printf("error: invalid option %s\n", argv[i]);
            print_help();
            exit(EXIT_FAILURE);
        }
    }

    printf("Initializing\n");
    setlocale(LC_ALL, ""); // Set locale so unicode characters work properly
    init_ncurses();

    // Main loop
    while (true)
    {
        if (screensaver_mode && getch() != ERR)
        {
            break;
        }
        else if (!screensaver_mode)
        {
            int c = getch();
            switch (c) {
                case KEY_UP:
                    smooth_factor += 0.05;
                    break;
                case KEY_DOWN:
                    smooth_factor -= 0.05;
                    break;
            }

            smooth_factor = CLAMP(smooth_factor, 0, 1);
        }

        draw_vumeter_data(audio.left_channel_dbs, audio.right_channel_dbs);
        usleep(1000);
    }

    if (pthread_join(audio_thread, NULL) != 0) {
        fprintf(stderr, "Error");
        return EXIT_FAILURE;
    }

    printf("Audio finished\n");
    cleanup_ncurses();

    exit(EXIT_SUCCESS);
}

void print_help()
{
    printf("%s",
        "Usage: vumz [OPTION]...\n"
        "\n"
        "vumz is a simple cli vumeter."
        "\n"
        "Options:\n"
        "   -D, --debug         debug mode: print useful data for devs\n"
        "   -h, --help          show help\n"
        "   -S, --screensaver   screensaver mode: press any key to quit\n"
        "\n"
    );
}

