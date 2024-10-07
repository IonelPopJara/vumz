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

static double framerate = 60.0;
static double noise_reduction = 77.0;
static float sensitivity = 0.5;

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
    audio.sensitivity = sensitivity;
    audio.mem[0] = -60.0f;
    audio.mem[1] = -60.0f;
    audio.terminate = 0;
    audio.debug = 0;
    audio.color_theme = 2;

    pthread_mutex_init(&audio.lock, NULL);

    // Create a thread to run the input function
    if (pthread_create(&audio_thread, NULL, input_pipewire, (void*)&audio) != 0) {
        fprintf(stderr, "Error");
        return EXIT_FAILURE;
    }

    bool screensaver_mode = false;

    // Check command line arguments
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-D") == 0 || strcmp(argv[i], "--debug") == 0)
        {
            audio.debug = 1;
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
            handle_sigint(0);
            break;
        }
        else if (!screensaver_mode)
        {
            int c = getch();
            switch (c) {
                case KEY_UP:
                    audio.noise_reduction += 1.0;
                    break;
                case KEY_DOWN:
                    audio.noise_reduction -= 1.0;
                    break;
                case KEY_LEFT:
                    audio.color_theme -= 1;
                    if (audio.color_theme < 0) audio.color_theme = 6;
                    break;
                case KEY_RIGHT:
                    audio.color_theme = (audio.color_theme + 1) % 7;
                    break;
                case 'd':
                    audio.debug = audio.debug == 1 ? 0 : 1;
                    break;
		case 43: // Plus sign (+)
		    audio.sensitivity += 0.1;
		    break;
		case 45: // Minus sign (-)
		    audio.sensitivity -= 0.1;
		    break;
                case 'q': // Quit on 'q'
                case 27: // Escape key (ASCII 27)
                    handle_sigint(0);
                    break;
            }

            audio.noise_reduction = CLAMP(audio.noise_reduction, 0, 200);
	    audio.sensitivity = CLAMP(audio.sensitivity, 0.0, 1.0);
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
           "CLI VU Meter Visualizer.\n"
           "Options:\n"
           "\t-D, --debug\t\tdebug mode: print useful data\n"
           "\t-h, --help\t\tshow help\n"
           "\t-S, --screensaver\tscreensaver mode: press any key to quit\n"
           "\n"
           "Keys:\n"
           "\tLeft\tSwitch to previous color theme\n"
           "\tRight\tSwitch to next color theme\n"
           "\tUp\tIncrease noise reduction\n"
           "\tDown\tDecrease noise reduction\n"
	   "\tPlus\tIncrease sensitivity\n"
	   "\tMinus\tDecrease sensitivity\n"
	   "\td\tToggle debug mode\n"
           "\tq\tQuit\n"
           "\tEscape\tQuit\n"
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
