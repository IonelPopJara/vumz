#ifndef AUDIO_CAP_H
#define AUDIO_CAP_H

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

/* In order to properly process the data from the stream
 * and display it on the VU Meter, I have defined this struct
 * which will hold the stream, the loop, and the audio format,
 * and the necessary audio parameters for the VU Meter.
 * information.
*/
struct pipewire_data {
    struct pw_main_loop *loop;
    struct pw_stream *stream;
    struct spa_audio_info format;

    struct audio_data *audio;
};

struct audio_data {
    int n_channels;
    float left_channel_dbs;
    float right_channel_dbs;
};

void *input_pipewire(void *audiodata);

#endif // AUDIO_CAP_H

