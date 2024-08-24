/*
 * VUMZ (VU Meter visualiZer)
 * TODO:
 *  - [x] Add a buffer to store previous frames
 *  - [ ] Add a starting line that never dissapears
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
#include <math.h>
#include "audio-cap.h"
#include "audio-out.h"

#define CLAMP(val, min, max) (val < min ? min : (val > max ? max : val))

void print_help();
long long current_time_in_ns();
void apply_smoothing(struct audio_data *audio);

float curr[2];
float prev[2];

static double framerate = 60.0;
static double noise_reduction = 77.0;
double gravity_mod = 0.12;

int main(int argc, char **argv)
{
    /*gravity_mod = pow((60.0 / framerate), 2.5) * 1.54 / noise_reduction;*/
    /*if (gravity_mod < 1) {*/
    /*    gravity_mod = 1;*/
    /*}*/

    pthread_t audio_thread;
    struct audio_data audio = {};
    audio.noise_reduction = noise_reduction;

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

        /*if (screensaver_mode && getch() != ERR)*/
        /*{*/
        /*    break;*/
        /*}*/
        /*else if (!screensaver_mode)*/
        /*{*/
        /*    int c = getch();*/
        /*    switch (c) {*/
        /*        case KEY_UP:*/
        /*            gravity_mod += 0.1;*/
        /*            break;*/
        /*        case KEY_DOWN:*/
        /*            gravity_mod -= 0.1;*/
        /*            break;*/
        /*    }*/
        /**/
        /*    audio.noise_reduction = CLAMP(audio.noise_reduction, 0, 200);*/
        /*}*/

        // Lock the audio data before processing
        pthread_mutex_lock(&audio.lock);

        // Apply smoothing
        apply_smoothing(&audio);

        // Unlock the audio data after processing
        pthread_mutex_unlock(&audio.lock);

        // Draw vumeter data
        draw_vumeter_data(audio.audio_out_buffer, audio.audio_out_buffer_prev, audio.n_channels, gravity_mod);

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

void apply_smoothing(struct audio_data *audio) {
    /*
     * This smoothing function was adapted from cava:
     * https://github.com/karlstav/cava/blob/master/cavacore.c
     */

    for (int n = 0; n < audio->n_channels; n++) {
        // Since the audio buffers contain the data in decibels
        // the range of these values can be [-60, 0]
        float curr_val = audio->audio_out_buffer[n];
        float prev_val = audio->audio_out_buffer[n];

        // Apply gravity effect if current value is less than previous
        // This works with decibels, so for example:
        // If the curr_val is -23db and prev_val was -5db
        if (curr_val < prev_val && audio->noise_reduction > 0.1) {
            /* NOTE:
             * fall: A value that keeps increasing to simulate gravity, if the current value is bigger than the previous one, it resets to 0
             * peak: If the current value is bigger than the previous value, we assign the current value to peak
             * mem: The previous value used for the integral smoothing
             */
            curr_val = audio->peak[n] * (1.0 - (audio->fall[n] * audio->fall[n] * gravity_mod));

            audio->fall[n] += 0.028;
        } else {
            audio->peak[n] = curr_val;
            audio->fall[n] = 0.0;
        }

        // Update the previous frame's buffer for next iteration
        audio->audio_out_buffer_prev[n] = curr_val;

        // I guess the final touch is the integral smoothing which is just the memory
        curr_val = audio->mem[n] * audio->noise_reduction + curr_val;
        audio->mem[n] = curr_val;
    }
}

