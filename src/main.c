/*
 * VUMZ (VU Meter visualiZer)
 * TODO:
 * - [x] Implement color changes
 * - [x] Fix ncurses and pipewire not closing properly
 * - [x] Optimize rendering cycle
 * - [ ] Add 3d effect
 * - [ ] Improve sensitivity
 */

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <locale.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include "audio-cap.h"
#include "audio-out.h"

#define CLAMP(val, min, max) (val < min ? min : (val > max ? max : val))

void print_help();
long long current_time_in_ns();
void handle_sigint(int sig);

float curr[2];
float prev[2];

static double framerate = 60.0;
static double noise_reduction = 77.0;
double gravity_mod = 0.12;

int main(int argc, char **argv)
{
    signal(SIGINT, handle_sigint); // Set up signal interrupt

    pthread_t audio_thread;
    struct audio_data audio = {};
    audio.noise_reduction = noise_reduction;
    audio.framerate = framerate;
    audio.audio_out_buffer_prev[0] = -60.0f;
    audio.audio_out_buffer_prev[1] = -60.0f;
    audio.audio_out_buffer[0] = -60.0f;
    audio.audio_out_buffer[1] = -60.0f;
    audio.mem[0] = -60.0f;
    audio.mem[1] = -60.0f;
    audio.terminate = 0;
    audio.wasThisShitWorking = 0;
    audio.color_theme = 2;

    pthread_mutex_init(&audio.lock, NULL);

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
                    gravity_mod += 0.1;
                    break;
                case KEY_DOWN:
                    gravity_mod -= 0.1;
                    break;
                case KEY_LEFT:
                    audio.color_theme -= 1;
                    if (audio.color_theme < 0) audio.color_theme = 6;
                    break;
                case KEY_RIGHT:
                    audio.color_theme = (audio.color_theme + 1) % 7;
                    break;
            }

            audio.noise_reduction = CLAMP(audio.noise_reduction, 0, 200);
        }

        // Draw vumeter data
        draw_vumeter_data(&audio);

        long long frame_duration_ns = current_time_in_ns() - start_time_ns;

        if (frame_duration_ns < target_frame_time_ns) {
            usleep((target_frame_time_ns - frame_duration_ns) / 1000); // sleep for the remaining time in us
        }
    }

    if (pthread_join(audio_thread, NULL) != 0) {
        fprintf(stderr, "Error");
        return EXIT_FAILURE;
    }
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

void handle_sigint(int sig) {
    cleanup_ncurses();
    printf("Thank you for using vumz :)\n");
    exit(EXIT_SUCCESS);
}

