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
    float peak[2];
    float fall[2];
    float mem[2];
    double noise_reduction;
    pthread_mutex_t lock;
};

void *input_pipewire(void *audiodata);

#endif // AUDIO_CAP_H

