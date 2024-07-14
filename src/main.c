/*
 * VUMZ (VU Meter visualiZer)
 */

/* TODO:
    * [ ] Add debug flag
    * [ ] Add screensaver mode
    * [ ] Fix scaling
    * [ ] Add sensitivity adjustment
 */

#include <ncurses.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <locale.h>

#include <spa/param/audio/format-utils.h>
#include <pipewire/pipewire.h>

#define DEBUG_MIN_HEIGHT 7
#define DEBUG_MIN_WIDTH 25

#define VUMETER_GREEN_THRESHOLD_DB -30.0f
#define VUMETER_YELLOW_THRESHOLD_DB -15.0f

/* VARIABLES */
int vumeter_border_height = 0;

int current_left_volume_height = 0;
int current_right_volume_height = 0;

float raw_left_channel_amplitude = 0.0f;
float raw_right_channel_amplitude = 0.0f;

int green_threshold_height = 0;
int yellow_threshold_height = 0;

float sensitivity = 150.0f;
/* VARIABLES */

/* UTIL */
static int db_to_vu_height(float db, int vu_height);
/* UTIL */

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

static void on_stream_param_changed(void *_data, uint32_t id, const struct spa_pod *param);

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .param_changed = on_stream_param_changed,
    .process = on_process,
};

static void do_quit(void *userdata, int signal_number);
/* PIPEWIRE */

int main(int argc, char **argv)
{
    setlocale(LC_ALL, ""); // Set locale so unicode characters work properly

    /* Pipewire config */
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

    // Create a simple stream
    props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                              PW_KEY_CONFIG_NAME, "client-rt.conf",
                              PW_KEY_MEDIA_CATEGORY, "Capture",
                              PW_KEY_MEDIA_ROLE, "Music",
                              NULL);

    pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");  // Enable capture from the sink monitor ports

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
    /* Pipewire config */

    /* Ncurses config */
    WINDOW *win_debug; // Window that will display debug information when the verbose flag is enabled
    WINDOW *win_vumeter; // Window that will display the VU Meter

    short xpos_win_offset = 2; // Offset for the x position of the windows
    short ypos_win_offset = 1; // Offset for the y position of the windows

    int height_win_debug, width_win_debug, starty_win_debug, startx_win_debug;
    int height_win_vumeter, width_win_vumeter, starty_win_vumeter, startx_win_vumeter;

    initscr();              /* Start curses mode */
    raw();                  /* Line buffering disabled */
    keypad(stdscr, TRUE);   /* Enable keypad */
    noecho();               /* Don't echo user input */
    curs_set(0);            /* Make the cursor invisible */
    cbreak();               /* Don't wait for new lines to read input */
    start_color();          /* Start color */

    init_pair(1, COLOR_GREEN, COLOR_BLACK); // First color pair, this will be used for the lower volume levels
    init_pair(2, COLOR_YELLOW, COLOR_BLACK); // Second color pair, this will be used for the medium volume levels
    init_pair(3, COLOR_RED, COLOR_BLACK); // Third color pair, this will be used for the higher volume levels
    /* Ncurses config */

    // Define height, width, and starting position for the debug window
    height_win_debug = LINES / 5;
    width_win_debug = COLS / 2;
    starty_win_debug = ypos_win_offset;
    startx_win_debug = xpos_win_offset;

    // Define height, width, and starting position for the vumeter window
    height_win_vumeter = LINES - 2;
    width_win_vumeter = COLS - 4;
    starty_win_vumeter = ypos_win_offset;
    startx_win_vumeter = xpos_win_offset;

    refresh(); // Refresh the screen

    // NOTE: The rendering order matters
    // Create the vumeter window
    win_vumeter = create_newwin(height_win_vumeter, width_win_vumeter, starty_win_vumeter, startx_win_vumeter);
    // Create the debug window
    win_debug = create_newwin(height_win_debug, width_win_debug, starty_win_debug, startx_win_debug);

    draw_debug_info(win_debug, 2, height_win_debug, width_win_debug);

    // Calculate vumeters coordinates
    vumeter_border_height = height_win_vumeter - 2;
    int vumeter_border_width = 2;

    int vumeter_border_starty = height_win_vumeter - 1;
    int left_vumeter_border_startx = (width_win_vumeter / 2) - vumeter_border_width - 1;
    int right_vumeter_border_startx = (width_win_vumeter / 2) + vumeter_border_width + 1;

    // Calculate the thresholds for the VU Meter
    green_threshold_height = db_to_vu_height(VUMETER_GREEN_THRESHOLD_DB, vumeter_border_height);
    yellow_threshold_height = db_to_vu_height(VUMETER_YELLOW_THRESHOLD_DB, vumeter_border_height);

    // window, height, width, starting x, starting y
    draw_vumeter_border(win_vumeter, vumeter_border_height, vumeter_border_width, vumeter_border_starty, left_vumeter_border_startx);
    draw_vumeter_border(win_vumeter, vumeter_border_height, vumeter_border_width, vumeter_border_starty, right_vumeter_border_startx);

    while (true)
    {
        pw_loop_iterate(pw_main_loop_get_loop(data.loop), 0);

        draw_debug_info(win_debug, 2, raw_left_channel_amplitude, raw_right_channel_amplitude);

        draw_vumeter_data(win_vumeter, current_left_volume_height, vumeter_border_height - 1, vumeter_border_width, vumeter_border_starty - 1, left_vumeter_border_startx - 1);
        draw_vumeter_data(win_vumeter, current_right_volume_height, vumeter_border_height - 1, vumeter_border_width, vumeter_border_starty - 1, right_vumeter_border_startx - 1);
    }

    // Destroy windows
    destroy_win(win_debug);
    destroy_win(win_vumeter);
    endwin();

    // Stop pipewire stream
    pw_stream_destroy(data.stream);
    pw_main_loop_destroy(data.loop);
    pw_deinit();

    return 0;
}

/* UTIL */
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
/* UTIL */

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
        // Apply color here according to db
        if (i < data)
        {
            if (i < green_threshold_height)
            {
                //green
                wattron(local_win, COLOR_PAIR(1));
                // Draw a "▒▒";
                mvwprintw(local_win, starty - i, startx, "▒▒");
                wattroff(local_win, COLOR_PAIR(1));
            }
            else if (i < yellow_threshold_height)
            {
                //yellow
                wattron(local_win, COLOR_PAIR(2));
                // Draw a "▒▒";
                mvwprintw(local_win, starty - i, startx, "▒▒");
                wattroff(local_win, COLOR_PAIR(2));
            }
            else
            {
                //yellow
                wattron(local_win, COLOR_PAIR(3));
                // Draw a "▒▒";
                mvwprintw(local_win, starty - i, startx, "▒▒");
                wattroff(local_win, COLOR_PAIR(3));
            }
        }
        else
        {
            // Otherwise, draw a ' '
            mvwprintw(local_win, starty - i, startx, "░░");
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

float lerp_factor = 0.1f;

int lerp(int start, int end, float percent)
{
    return start + percent + (end - start);
}

static void on_process(void *userdata)
{
    struct data *data = userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;
    float *samples, max;
    uint32_t c, n, n_channels, n_samples;

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
     * I don't think lerp is a good choice for smoothing the movement
     * but it'll have to do for now
     */
    for (c = 0; c < data->format.info.raw.channels; c++)
    {
        max = 0.0f;
        for (n = c; n < n_samples; n += n_channels)
        {
            max = fmaxf(max, fabsf(samples[n]));
        }

        if (c == 0)
        {
            // Store the amplitude for debugging purposes
            raw_left_channel_amplitude = max;

            // Smooth out values for the left channel so it doesn't jump around too much
            int target_left_volume_height = db_to_vu_height(amplitude_to_db(max), vumeter_border_height);
            current_left_volume_height = lerp(current_left_volume_height, target_left_volume_height, lerp_factor);
        }
        else if (c == 1)
        {
            // Store the amplitude for debugging purposes
            raw_right_channel_amplitude = max;

            // Smooth out values for the right channel so it doesn't jump around too much
            int target_right_volume_height = db_to_vu_height(amplitude_to_db(max), vumeter_border_height);
            current_right_volume_height = lerp(current_right_volume_height, target_right_volume_height, lerp_factor);
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
