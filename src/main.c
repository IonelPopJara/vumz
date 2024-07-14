/*
 * VUMZ (VU Meter Audio Visualizer) 
 */

/* TODO:
 * [ ] Add smooth transition between values
 * [ ] Add sensitivity adjust
 * [x] Use actual math to calculate the volume scale
 */

#include <ncurses.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <spa/param/audio/format-utils.h>
#include <pipewire/pipewire.h>

#define DEBUG_MIN_HEIGHT 7
#define DEBUG_MIN_WIDTH 25

/* GLOBAL VARIABLES */
int vumeter_border_height = 0;

int current_left_channel_volume = 0;
int current_right_channel_volume = 0;

int current_left_volume_height = 0;
int current_right_volume_height = 0;

float raw_left_channel_data = 0.0f;
float raw_right_channel_data = 0.0f;

float sensitivity = 150.0f;
/* GLOBAL VARIABLES */

/* NCURSES */
WINDOW *create_newwin(int height, int width, int starty, int startx);

void destroy_win(WINDOW *local_win);

void draw_debug_info(WINDOW *local_win, int channels, float left, float right);

void draw_vumeter_border(WINDOW* local_win, int height, int width, int starty, int startx);

void draw_vumeter_data(WINDOW* local_win, int data, int height, int width, int starty, int startx);

void cleanup_ncurses();
/* NCURSES */

/* PIPEWIRE */
struct data {
    struct pw_main_loop *loop;
    struct pw_stream *stream;

    struct spa_audio_info format;
    unsigned move:1;
};

static void on_process(void *userdata);

/* Be notified when the stream param changes. We're only looking at
 * the format changes.
 */
static void on_stream_param_changed(void *_data, uint32_t id, const struct spa_pod *param);

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .param_changed = on_stream_param_changed,
    .process = on_process,
};

static void do_quit(void *userdata, int signal_number);
/* PIPEWIRE */

// Main (idk why I put this)
int main(int argc, char **argv)
{
    /* Pipewire */
    struct data data = { 0, };
    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct pw_properties *props;
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    pw_init(&argc, &argv);

    // Make a main loop
    data.loop = pw_main_loop_new(NULL /* properties */);

    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGINT, do_quit, &data);
    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGTERM, do_quit, &data);

    // Create a simplel stream
    props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                              PW_KEY_CONFIG_NAME, "client-rt.conf",
                              PW_KEY_MEDIA_CATEGORY, "Capture",
                              PW_KEY_MEDIA_ROLE, "Music",
                              NULL);

    // To capture from the sink monitor ports
    pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");

    data.stream = pw_stream_new_simple(
        pw_main_loop_get_loop(data.loop),
        "audio-capture",
        props,
        &stream_events,
        &data);

    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
                    &SPA_AUDIO_INFO_RAW_INIT(
                    .format = SPA_AUDIO_FORMAT_F32));

    // Connec this stream
    pw_stream_connect(data.stream,
                      PW_DIRECTION_INPUT,
                      PW_ID_ANY,
                      PW_STREAM_FLAG_AUTOCONNECT |
                      PW_STREAM_FLAG_MAP_BUFFERS |
                      PW_STREAM_FLAG_RT_PROCESS,
                      params, 1);
    /* Pipewire */

    WINDOW *win_debug;
    WINDOW *win_vumeter;

    short xpos_offset = 2;
    short ypos_offset = 1;

    int height_debug, width_debug, starty_debug, startx_debug;
    int height_vumeter, width_vumeter, starty_vumeter, startx_vumeter;

    initscr();              /* Start curses mode */
    raw();                  /* Line buffering disabled */
    keypad(stdscr, TRUE);   /* Enable keypad */
    noecho();               /* Don't echo user input */
    curs_set(0);            /* Make the cursor invisible */
    cbreak();               /* Don't wait for new lines to read input */
    start_color();          /* Start color */

    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_RED, COLOR_BLACK);

    // Define height, width, and starting position for the debug window
    height_debug = LINES / 5;
    width_debug = COLS / 2;
    starty_debug = ypos_offset;
    startx_debug = xpos_offset;

    // Define height, width, and starting position for the vumeter window
    height_vumeter = LINES - 2;
    width_vumeter = COLS - 4;
    starty_vumeter = ypos_offset;
    startx_vumeter = xpos_offset;

    refresh();

    // NOTE: The rendering order matters

    // Create the vumeter window
    win_vumeter = create_newwin(height_vumeter, width_vumeter, starty_vumeter, startx_vumeter);

    // Create the debug window
    win_debug = create_newwin(height_debug, width_debug, starty_debug, startx_debug);

    draw_debug_info(win_debug, 2, height_debug, width_debug);

    // Calculate vumeters coordinates
    // NOTE: this is the height of the vumeter itself, not the vumeter window
    // This is confusing and will need refactoring
    // NOTE: #2 I should probably account for scaling again...
    vumeter_border_height = height_vumeter - 2;
    int vumeter_border_width = 2;

    int vumeter_border_starty = height_vumeter - 1;
    int LVumeter_border_startx = (width_vumeter / 2) - vumeter_border_width - 1;
    int RVumeter_border_startx = (width_vumeter / 2) + vumeter_border_width + 1;

    // window, height, width, starting x, starting y
    draw_vumeter_border(win_vumeter, vumeter_border_height, vumeter_border_width, vumeter_border_starty, LVumeter_border_startx);
    draw_vumeter_border(win_vumeter, vumeter_border_height, vumeter_border_width, vumeter_border_starty, RVumeter_border_startx);

    //TODO: Check here for audio buffer
    while (1)
    {

        pw_loop_iterate(pw_main_loop_get_loop(data.loop), 0);

        // Using a random number just to test
        // int LVolume_data = rand() % (LINES - 2);
        // int RVolume_data = rand() % (LINES - 2);
        //current_right_volume_height

        // draw_debug_info(win_debug, 2, current_left_volume_height, current_left_volume_height);
        draw_debug_info(win_debug, 2, raw_left_channel_data, raw_right_channel_data);

        //TODO: Account for scaling
        // attron(COLOR_PAIR(1));

        draw_vumeter_data(win_vumeter, current_left_volume_height, vumeter_border_height - 1, vumeter_border_width, vumeter_border_starty - 1, LVumeter_border_startx - 1);
        draw_vumeter_data(win_vumeter, current_right_volume_height, vumeter_border_height - 1, vumeter_border_width, vumeter_border_starty - 1, RVumeter_border_startx - 1);

        // attroff(COLOR_PAIR(1));
        // usleep(80000);
    }

    refresh();
    getch();

    // Destroy windows
    destroy_win(win_debug);
    destroy_win(win_vumeter);
    endwin();

    pw_stream_destroy(data.stream);
    pw_main_loop_destroy(data.loop);
    pw_deinit();

    return 0;
}

/* NCURSES */
WINDOW *create_newwin(int height, int width, int starty, int startx)
{
    WINDOW* local_win;
    local_win = newwin(height, width, starty, startx);
    // box(local_win, 0, 0);
    wborder(local_win, '+',  '+', '+', '+', '+', '+', '+', '+');
    wrefresh(local_win);

    return local_win;
}

void destroy_win(WINDOW* local_win)
{
    wborder(local_win, ' ',  ' ', ' ', ' ', ' ', ' ', ' ', ' ');
    wrefresh(local_win);
    delwin(local_win);
}

void draw_debug_info(WINDOW *local_win, int channels, float left, float right)
{
    mvwprintw(local_win, 1, 2, "Audio Visualizer");
    mvwprintw(local_win, 2, 2, "----------------");
    mvwprintw(local_win, 3, 2, "Channels: %d", channels);
    mvwprintw(local_win, 4, 2, "|- L: %f", left);
    mvwprintw(local_win, 5, 2, "|- R: %f", right);
    wrefresh(local_win);
}

void draw_vumeter_border(WINDOW* local_win, int height, int width, int starty, int startx)
{ 
    int i = 0;

    // Draw the border of the vumeter
    for (int j = 1; j <= width; j++)
    {
        mvprintw(starty, startx + j, "-");
    }
    for (i = 0; i < height; i++)
    {
        mvprintw(starty - i, startx, "|");
        mvprintw(starty - i, startx + width + 1, "|");
    }
    for (int j = 1; j <= width; j++)
    {
        mvprintw(starty - i + 1, startx + j, "-");
    }

    refresh();
}

void draw_vumeter_data(WINDOW* local_win, int data, int height, int width, int starty, int startx)
{
    for (int i = 0; i < height; i++)
    {
        // TODO: Apply color here according to i

        if (i < data)
        {
            if (i < 7)
            {
                //green
                wattron(local_win, COLOR_PAIR(1));
                // Draw a '*'
                mvwprintw(local_win, starty - i, startx, "**");
                wattroff(local_win, COLOR_PAIR(1));
            }
            else
            {
                //yellow
                wattron(local_win, COLOR_PAIR(2));
                // Draw a '*'
                mvwprintw(local_win, starty - i, startx, "**");

                wattroff(local_win, COLOR_PAIR(2));
            }

        }
        else
        {
            // Otherwise, draw a ' '
            mvwprintw(local_win, starty - i, startx, "  ");
        }
    }

    wrefresh(local_win);
}

void cleanup_ncurses()
{
    endwin();
}
/* NCURSES */

/* PIPEWIRE */

static float amplitude_to_db(float amplitude)
{
    if (amplitude <= 0.0f)
    {
        return -60.0f;
    }

    return 20.0f * log10f(amplitude);
}

static int db_to_vu_height(float db, int vu_height)
{
    if (db <= -60.0f)
    {
        db = -60.0f;
    }

    if (db >= 0.0f)
    {
        db = 0.0f;
    }

    return (int)(((db + 60.0f) / 60.0f) * vu_height);
}
static void on_process(void *userdata)
{
    struct data *data = userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;
    float *samples, max;
    uint32_t c, n, n_channels, n_samples, peak;

    if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL)
    {
        pw_log_warn("out of bufers: %m");
        return;
    }

    buf = b->buffer;
    if ((samples = buf->datas[0].data) == NULL)
    {
        return;
    }

    n_channels = data->format.info.raw.channels;
    n_samples = buf->datas[0].chunk->size / sizeof(float);

    /* NOTE:
     * I don't know if I missed something in the documentation but the
     * amplitude of each sample seems to be in the interval [-0.5, 0.5].
     *
     * In theory, it would make sense for it to go from [-1, 1] but there
     * has to be some preprocessing done to it. Or maybe it's just like this
     * because the audio signals are not "strong" enough, which is good.
     */
    for (c = 0; c < data->format.info.raw.channels; c++)
    {
        max = 0.0f;
        for (n = c; n < n_samples; n += n_channels)
        {
            max = fmaxf(max, fabsf(samples[n]));
        }

        // Normalized value
        peak = SPA_CLAMP(max * sensitivity, 0, vumeter_border_height);

        if (c == 0)
        {
            current_left_channel_volume = peak;
            raw_left_channel_data = max;

            // TODO: add lerp here
            current_left_volume_height = db_to_vu_height(amplitude_to_db(max), vumeter_border_height);
        }
        else if (c == 1)
        {
            current_right_channel_volume = peak;
            raw_right_channel_data = max;

            // TODO: add lerp here
            current_right_volume_height = db_to_vu_height(amplitude_to_db(max), vumeter_border_height);
        }
    }

    data->move = true;
    fflush(stdout);

    pw_stream_queue_buffer(data->stream, b);
}
/* [on_process] */

/* Be notified when the stream param changes. We're only looking at
 * the format changes.
 */
static void on_stream_param_changed(void *_data, uint32_t id, const struct spa_pod *param)
{
    struct data *data = _data;

    /* NULL means to clear the format */
    if (param == NULL || id != SPA_PARAM_Format)
    {
        return;
    }

    if (spa_format_parse(param, &data->format.media_type, &data->format.media_subtype) < 0)
    {
        return;
    }

    /* only accept raw audio */
    if (data->format.media_type != SPA_MEDIA_TYPE_audio ||
        data->format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
    {
        return;
    }

    /* call a helper function to parse the format for us */
    spa_format_audio_raw_parse(param, &data->format.info.raw);

    // fprintf(stdout, "capturing rate:%d channels:%d\n",
            // data->format.info.raw.rate, data->format.info.raw.channels);
}

static void do_quit(void *userdata, int signal_number)
{
    struct data* data = userdata;
    pw_main_loop_quit(data->loop);
    cleanup_ncurses();

    pw_stream_destroy(data->stream);
    pw_main_loop_destroy(data->loop);
    pw_deinit();

    exit(EXIT_SUCCESS);
}
/* PIPEWIRE */

