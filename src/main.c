/*
 * VUMZ (VU Meter visualiZer)
 * TODO:
 *  - [ ] Add a starting line that never dissapears
 *  - [ ] Add a buffer to store previous frames
 *  - [ ] Add gravity simulation? or some smoothing
 */

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <locale.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include "audio-cap.h"
#include "audio-out.h"

#define CLAMP(val, min, max) (val < min ? min : (val > max ? max : val))

float smooth_factor = 1.0f;

void print_help();
long long current_time_in_ns();

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

    const long long target_frame_time_ns = 16666666LL; // 16.67 milliseconds in nanoseconds (1/60 in ms)

    // Main loop
    while (true)
    {
        long long start_time_ns = current_time_in_ns();

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

        long long frame_duration_ns = current_time_in_ns() - start_time_ns;

        if (frame_duration_ns < target_frame_time_ns) {
            usleep((target_frame_time_ns - frame_duration_ns) / 1000); // sleep for the remaining time in us
        }
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

long long current_time_in_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

