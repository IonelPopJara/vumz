/*
 * Audio capture program
 */

#include "audio-cap.h"
#include <math.h>

static float amplitude_to_db(float amplitude)
{
    if (amplitude <= 0.0f)
    {
        return -60.0f;
    }

    return 20.0f * log10f(amplitude);
}

static void on_process(void *userdata) {
    struct pipewire_data *data = userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;
    float *samples, max;
    uint32_t c, n, n_channels, n_samples;

    // TODO: Add a terminate bool here for the vumz struct

    if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL) {
        pw_log_warn("out of bufers: %m");
        return;
    }

    buf = b->buffer;
    if ((samples = buf->datas[0].data) == NULL) {
        return;
    }

    n_channels = data->format.info.raw.channels;
    n_samples = buf->datas[0].chunk->size / sizeof(float);
    /*printf("Inside the thread\n");*/

    // Write to input buffer
    // Cast the data
    struct audio_data* audio = (struct audio_data*)(data->audio);

    /*printf("Audio data:\n");*/
    for (c = 0; c < n_channels; c++) {
        max = 0.0f;
        for (n = c; n < n_samples; n += n_channels) {
            max = fmaxf(max, fabsf(samples[n]));
        }

        if (c == 0) {
            float left_channel_dbs = amplitude_to_db(max);
            /*printf("\t%.6f -- L\n", left_channel_dbs);*/
            /*data->left_channel_dbs = left_channel_dbs;*/
            audio->left_channel_dbs = left_channel_dbs;
        }
        else if (c == 1) {
            float right_channel_dbs = amplitude_to_db(max);
            /*printf("\t%.6f -- R\n", right_channel_dbs);*/
            /*data->right_channel_dbs = right_channel_dbs;*/
            audio->right_channel_dbs = right_channel_dbs;
        }
    }

    pw_stream_queue_buffer(data->stream, b);
}

static void on_stream_param_changed(void *_data, uint32_t id, const struct spa_pod *param) {
    struct pipewire_data *data = _data;

    /* NULL means to clear the format */
    if (param == NULL || id != SPA_PARAM_Format) {
        return;
    }

    if (spa_format_parse(param, &data->format.media_type, &data->format.media_subtype) < 0) {
        return;
    }

    /* only accept raw audio */
    if (data->format.media_type != SPA_MEDIA_TYPE_audio ||
        data->format.media_subtype != SPA_MEDIA_SUBTYPE_raw) {
        return;
    }

    /* call a helper function to parse the format for us */
    spa_format_audio_raw_parse(param, &data->format.info.raw); }

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .param_changed = on_stream_param_changed,
    .process = on_process,
};

static void do_quit(void *userdata, int signal_number) {
    struct pipewire_data* data = userdata;
    pw_main_loop_quit(data->loop);

    pw_stream_destroy(data->stream);
    pw_main_loop_destroy(data->loop);
    pw_deinit();

    exit(EXIT_SUCCESS);
}

void *input_pipewire(void *audiodata) {
    struct pipewire_data data = { 0, };
    data.audio = (struct audio_data*)audiodata;
    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct pw_properties *props;
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    pw_init(0, 0);

    // Make a main loop
    data.loop = pw_main_loop_new(NULL /* properties */);
    // TODO: Add check
    // if (data.loop == NULL) {}

    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGINT, do_quit, &data);
    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGTERM, do_quit, &data);

    // Create a simple stream
    props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                              PW_KEY_CONFIG_NAME, "client-rt.conf",
                              PW_KEY_MEDIA_CATEGORY, "Capture",
                              PW_KEY_MEDIA_ROLE, "Music",
                              NULL);

    pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");  // Enable capture from the sink monitor ports
    // TODO: Configure more properties

    data.stream = pw_stream_new_simple(
        pw_main_loop_get_loop(data.loop),
        "audio-capture",
        props,
        &stream_events,
        &data);

    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
                                           &SPA_AUDIO_INFO_RAW_INIT(
                                           .format = SPA_AUDIO_FORMAT_F32));

    // Connect this stream
    pw_stream_connect(data.stream,
                      PW_DIRECTION_INPUT,
                      PW_ID_ANY,
                      PW_STREAM_FLAG_AUTOCONNECT |
                      PW_STREAM_FLAG_MAP_BUFFERS |
                      PW_STREAM_FLAG_RT_PROCESS,
                      params, 1);

    pw_main_loop_run(data.loop);

    // Stop pipewire stream
    pw_stream_destroy(data.stream);
    pw_main_loop_destroy(data.loop);
    pw_deinit();

    return 0;
}

