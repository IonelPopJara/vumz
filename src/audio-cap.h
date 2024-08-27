#ifndef AUDIO_CAP_H
#define AUDIO_CAP_H

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <pthread.h>

/*
 * Main pipewire struct that holds the loop, the stream, and the audio format.
 * It also holds a custom struct audio_data which stores relevant info for
 * drawing the vumeter.
 *
 */
struct pipewire_data {
    struct pw_main_loop *loop;
    struct pw_stream *stream;
    struct spa_audio_info format;

    struct audio_data *audio;
};

/*
 * Custom struct that holds the number of channels, the two buffers of size 2.
 * The buffers are used to store the previous and current output data for the vumeter.
 */
struct audio_data {
    int n_channels;     // Number of channels (buffer size)
    float audio_out_buffer[2];   // Output buffer of size 2 (2 channels)
    float audio_out_buffer_prev[2];   // Previous output buffer
    float peak[2];      // For smoothing
    float fall[2];      // For smoothing
    float mem[2];       // For smoothing
    double noise_reduction; // For smoothing
    double framerate;   // Stores the framerate of the vumeter
    pthread_mutex_t lock; // For synchronization
    int terminate;      // To terminate audio thread
    int debug; // Boolean to debug stuff
    int color_theme; // Integer within a range to determine the color theme
};

void *input_pipewire(void *audiodata);

#endif // AUDIO_CAP_H

