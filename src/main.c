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
#include <argp.h>
#include "audio-cap.h"
#include "audio-out.h"

#define CLAMP(val, min, max) (val < min ? min : (val > max ? max : val))

// Required argp variables for program information
const char *argp_program_version = "vumz 1.0";
const char *argp_program_bug_address = "ionelalexandru.pop@gmail.com";

// Program documentation
static char doc[] = "VUMZ -- CLI VU Meter Visualizer\v"
                    "Keys:\n"
                    "\tLeft\tSwitch to previous color theme\n"
                    "\tRight\tSwitch to next color theme\n"
                    "\tUp\tIncrease noise reduction\n"
                    "\tDown\tDecrease noise reduction\n"
                    "\td\tToggle debug mode\n"
                    "\tq\tQuit\n"
                    "\tEscape\tQuit";

static char args_doc[] = "";

// Command-line options for argp
static struct argp_option options[] = {
    {"debug",      'D', 0, 0, "Debug mode: print useful data"},
    {"screensaver",'S', 0, 0, "Screensaver mode: press any key to quit"},
    {0}
};

// Structure to store parsed arguments
struct arguments {
    bool debug_mode;
    bool screensaver_mode;
};

long long current_time_in_ns();
void handle_sigint(int sig);
void cleanup_resources(struct audio_data* audio, pthread_t* audio_thread, bool ncurses_initialized, bool mutex_initialized);

static double framerate = 60.0;
static double noise_reduction = 77.0;

// Callback function for parsing individual options
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;

    switch (key) {
        case 'D':
            arguments->debug_mode = true;
            break;
        case 'S':
            arguments->screensaver_mode = true;
            break;
        case ARGP_KEY_ARG:
            return 0;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

// argp parser configuration
static struct argp argp = {options, parse_opt, args_doc, doc};

int main(int argc, char **argv)
{
    signal(SIGINT, handle_sigint); // Set up signal interrupt

    // Initialize and parse command-line arguments
    struct arguments arguments = {
        .debug_mode = false,
        .screensaver_mode = false
    };
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

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
    audio.debug = arguments.debug_mode;
    audio.color_theme = 2;

    bool ncurses_initialized = false;
    bool mutex_initialized = false;

    // Initialize the mutex and check for errors
    if (pthread_mutex_init(&audio.lock, NULL) != 0) {
        fprintf(stderr, "Error initializing mutex\n");
        cleanup_resources(&audio, &audio_thread, ncurses_initialized, mutex_initialized);
        return EXIT_FAILURE;
    }
    mutex_initialized = true;

    // Create a thread to run the input function
    if (pthread_create(&audio_thread, NULL, input_pipewire, (void*)&audio) != 0) {
        fprintf(stderr, "Error creating audio thread\n");
        cleanup_resources(&audio, &audio_thread, ncurses_initialized, mutex_initialized);
        return EXIT_FAILURE;
    }

    printf("Initializing\n");
    setlocale(LC_ALL, ""); // Set locale so unicode characters work properly

    // Initialize ncurses and check for errors
    if (init_ncurses() != 0) {
        fprintf(stderr, "Error initializing ncurses\n");
        cleanup_resources(&audio, &audio_thread, ncurses_initialized, mutex_initialized);
        return EXIT_FAILURE;
    }
    ncurses_initialized = true;

    const long long target_frame_time_ns = 16666666LL; // 16.67 milliseconds in nanoseconds (1/60 in ms)

    // Main loop
    while (true)
    {
        long long start_time_ns = current_time_in_ns();

        if (arguments.screensaver_mode && getch() != ERR)
        {
            handle_sigint(0);
            break;
        }
        else if (!arguments.screensaver_mode)
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
                case 'q': // Quit on 'q'
                case 27: // Escape key (ASCII 27)
                    handle_sigint(0);
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

    // Clean up properly when the main loop ends
    cleanup_resources(&audio, &audio_thread, ncurses_initialized, mutex_initialized);

    return EXIT_SUCCESS;
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

// Cleanup function to release resources in case of errors
void cleanup_resources(struct audio_data* audio, pthread_t* audio_thread, bool ncurses_initialized, bool mutex_initialized) {
    // Clean up ncurses only if it was initialized
    if (ncurses_initialized) {
        cleanup_ncurses();
    }

    // Wait for the audio thread to finish if it was created
    if (audio_thread && audio->terminate == 0) {
        audio->terminate = 1;
        if (pthread_join(*audio_thread, NULL) != 0) {
            fprintf(stderr, "Error joining audio thread\n");
        }
    }

    // Destroy the mutex only if it was initialized
    if (mutex_initialized) {
        pthread_mutex_destroy(&audio->lock);
    }
}

