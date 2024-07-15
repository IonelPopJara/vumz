/*
 * VUMZ (VU Meter visualiZer)
 */

/* TODO:
    * [x] Add debug flag
    * [x] Add screensaver mode
    * [x] Fix scaling
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

#define DEBUG_WIN_HEIGHT 7;
#define DEBUG_WIN_WIDTH 19;

/* Structs */
/* In order to properly process the data from the stream
 * and display it on the VU Meter, I have defined this struct
 * which will hold the stream, the loop, and the audio format,
 * and the necessary audio parameters for the VU Meter.
 * information.
*/
struct pipewire_data {
    // Pipewire
    struct pw_main_loop *loop;
    struct pw_stream *stream;
    struct spa_audio_info format;
    // VU Meter
    int n_channels;
    float left_channel_dbs;
    float right_channel_dbs;
};

struct vu_meter {
    int border_height;
    int left_vumeter_height;
    int right_vumeter_height;
    int green_threshold_height;
    int yellow_threshold_height;
};

struct ncurses_window {
    WINDOW* win;
    int height;
    int width;
    int starty;
    int startx;
};
/* Structs */

int vumeter_border_height = 0;

int current_left_volume_height = 0;
int current_right_volume_height = 0;

int green_threshold_height = 0;
int yellow_threshold_height = 0;

float sensitivity = 150.0f;
/* VU Meter */

/* UTIL */
static int db_to_vu_height(float db, int vu_height);
/* UTIL */

/* NCURSES */
void init_ncurses();

WINDOW *create_newwin(int height, int width, int starty, int startx);

void destroy_win(WINDOW *local_win);

void draw_debug_info(WINDOW *local_win, void* vumeter_data);

void draw_vumeter_border(WINDOW* local_win, int height, int width, int starty, int startx);

void draw_vumeter_data(WINDOW* local_win, int data, int height, int width, int starty, int startx);

void cleanup_ncurses();
/* NCURSES */

/* PIPEWIRE */
static void on_process(void *vumeter_data);

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
    bool debug_mode = false;
    bool screensaver_mode = false;

    // Check command line arguments
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-D") == 0)
        {
            debug_mode = true;
        }
        else if (strcmp(argv[i], "-S") == 0)
        {
            screensaver_mode = true;
        }
    }

    setlocale(LC_ALL, ""); // Set locale so unicode characters work properly

    /* Pipewire config */
    struct pipewire_data data = { 0, };
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
    init_ncurses();
    /* Ncurses config */

    /* Ncurses windows */
    struct ncurses_window win_vumeter = {0};    // Window to render the vumeter
    struct ncurses_window win_debug = {0};      // Window to render the debug information

    // Initialize vumeter window
    win_vumeter.height = LINES;
    win_vumeter.width = COLS;
    win_vumeter.starty = 0;
    win_vumeter.startx = 0;

    refresh(); // Refresh the screen

    // NOTE: The rendering order matters
    // Create the vumeters window
    win_vumeter.win = create_newwin(win_vumeter.height, win_vumeter.width, win_vumeter.starty, win_vumeter.startx);

    if (debug_mode)
    {
        // Define height, width, and starting position for the debug window
        win_debug.height = DEBUG_WIN_HEIGHT;
        win_debug.width = DEBUG_WIN_WIDTH;
        win_debug.starty = 0;
        win_debug.startx = 0;
        // Create the debug window
        win_debug.win = create_newwin(win_debug.height, win_debug.width, win_debug.starty, win_debug.startx);
    }

    /* Vumeter Initialization */
    struct vu_meter vumeter = {0};
    /* Vumeter Initialization */

    // Calculate vumeters coordinates
    vumeter_border_height = win_vumeter.height;
    int vumeter_border_width = 2;

    int vumeter_border_starty = 0;
    int left_vumeter_border_startx = (win_vumeter.width / 2) - vumeter_border_width - 3;
    int right_vumeter_border_startx = (win_vumeter.width / 2) + vumeter_border_width - 1;

    // Calculate the thresholds for the VU Meter
    green_threshold_height = db_to_vu_height(VUMETER_GREEN_THRESHOLD_DB, vumeter_border_height);
    yellow_threshold_height = db_to_vu_height(VUMETER_YELLOW_THRESHOLD_DB, vumeter_border_height);

    // window, height, width, starting x, starting y
    draw_vumeter_border(win_vumeter.win, vumeter_border_height, vumeter_border_width, vumeter_border_starty, left_vumeter_border_startx);
    draw_vumeter_border(win_vumeter.win, vumeter_border_height, vumeter_border_width, vumeter_border_starty, right_vumeter_border_startx);

    while (true)
    {
        pw_loop_iterate(pw_main_loop_get_loop(data.loop), 0);

        if (debug_mode)
            draw_debug_info(win_debug.win, &data);

        draw_vumeter_data(win_vumeter.win, current_left_volume_height, vumeter_border_height, vumeter_border_width, vumeter_border_starty, left_vumeter_border_startx);
        draw_vumeter_data(win_vumeter.win, current_right_volume_height, vumeter_border_height, vumeter_border_width, vumeter_border_starty, right_vumeter_border_startx);

        if (screensaver_mode && getch() != ERR)
        {
            break;
        }
    }

    // Destroy windows
    destroy_win(win_debug.win);
    destroy_win(win_vumeter.win);
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

void init_ncurses()
{
    initscr();              /* Start curses mode */
    raw();                  /* Line buffering disabled */
    keypad(stdscr, TRUE);   /* Enable keypad */
    noecho();               /* Don't echo user input */
    curs_set(0);            /* Make the cursor invisible */
    cbreak();               /* Don't wait for new lines to read input */
    start_color();          /* Start color */
    nodelay(stdscr, TRUE);  /* Set getch() to be non-blocking */

    init_pair(1, COLOR_GREEN, COLOR_BLACK); // First color pair, this will be used for the lower volume levels
    init_pair(2, COLOR_YELLOW, COLOR_BLACK); // Second color pair, this will be used for the medium volume levels
    init_pair(3, COLOR_RED, COLOR_BLACK); // Third color pair, this will be used for the higher volume levels
    init_pair(4, COLOR_MAGENTA, COLOR_BLACK); // Fourth color pair, this will be used for the debug window
    init_pair(5, COLOR_BLUE, COLOR_BLACK); // Fifth color pair, this will be used for the debug window
}

WINDOW *create_newwin(int height, int width, int starty, int startx)
{
    WINDOW* local_win;
    local_win = newwin(height, width, starty, startx);
    // box(local_win, 0, 0);
    // wborder(local_win, '+',  '+', '+', '+', '+', '+', '+', '+');
    wborder(local_win, ' ',  ' ', ' ', ' ', ' ', ' ', ' ', ' ');
    wrefresh(local_win);

    return local_win;
}

void destroy_win(WINDOW* local_win)
{
    wborder(local_win, ' ',  ' ', ' ', ' ', ' ', ' ', ' ', ' ');
    wrefresh(local_win);
    delwin(local_win);
}

void draw_debug_info(WINDOW *local_win, void *vumeter_data)
{
    struct pipewire_data *data = (struct pipewire_data *)vumeter_data;
    wattron(local_win, COLOR_PAIR(4));
    mvwprintw(local_win, 1, 2, "VUMZ");
    mvwprintw(local_win, 2, 2, "---------------");
    wattroff(local_win, COLOR_PAIR(4));
    wattron(local_win, COLOR_PAIR(5));
    mvwprintw(local_win, 3, 2, "Channels: %d", data->n_channels);
    mvwprintw(local_win, 4, 2, "|- L: %.2f dB", data->left_channel_dbs);
    mvwprintw(local_win, 5, 2, "|- R: %.2f dB", data->right_channel_dbs);
    wattroff(local_win, COLOR_PAIR(5));
    wrefresh(local_win);
}

void draw_vumeter_border(WINDOW *local_win, int height, int width, int starty, int startx)
{
    int i = 0;

    // Draw the top border of the vumeter
    mvwaddch(local_win, starty, startx, ACS_ULCORNER);
    for (int j = 1; j <= width; j++)
    {
        mvwaddch(local_win, starty, startx + j, ACS_HLINE);
    }
    mvwaddch(local_win, starty, startx + width + 1, ACS_URCORNER);

    // Draw the left and right borders of the vumeter
    for (i = 1; i < height - 1; i++)
    {
        mvwaddch(local_win, starty + i, startx, ACS_VLINE);
        mvwaddch(local_win, starty + i, startx + width + 1, ACS_VLINE);
    }
    // Draw the bottom border of the vumeter
    mvwaddch(local_win, starty + i, startx, ACS_LLCORNER);
    for (int j = 1; j <= width; j++)
    {
        mvwaddch(local_win, starty + i, startx + j, ACS_HLINE);
    }
    mvwaddch(local_win, starty + i, startx + width + 1, ACS_LRCORNER);

    wrefresh(local_win);
}

void draw_vumeter_data(WINDOW* local_win, int data, int height, int width, int starty, int startx)
{
    startx++;
    starty++;
    for (int i = 0; i < height - 2; i++)
    {
        // Let's say data is 10, which means that we need to draw 10 lines and height is 20
        // We start from the top of the vumeter, and we have to draw the bottom 10 lines with color
        // if i = 0, height - i = 20, 20 > 10 (data), no color
        // if i = 10, height - i = 10, 10 = 10 (data), color
        // if i = 11, height - i = 9, 9 < 10 (data), color
        // therefore, as long as height - i < data, we need to apply color
        // Apply color according to db
        if (height - i < data)
        {
            if (height - i < green_threshold_height)
            {
                //green
                wattron(local_win, COLOR_PAIR(1));
                // Draw a "▒▒";
                mvwprintw(local_win, starty + i, startx, "▒▒");
                wattroff(local_win, COLOR_PAIR(1));
            }
            else if (height - i < yellow_threshold_height)
            {
                //yellow
                wattron(local_win, COLOR_PAIR(2));
                // Draw a "▒▒";
                mvwprintw(local_win, starty + i, startx, "▒▒");
                wattroff(local_win, COLOR_PAIR(2));
            }
            else
            {
                //red
                wattron(local_win, COLOR_PAIR(3));
                // Draw a "▒▒";
                mvwprintw(local_win, starty + i, startx, "▒▒");
                wattroff(local_win, COLOR_PAIR(3));
            }
        }
        else
        {
            mvwprintw(local_win, starty + i, startx, "░░");
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

static void on_process(void *vumeter_data)
{
    struct pipewire_data *data = vumeter_data;
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

    data->n_channels = n_channels;

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
            // Store the decibels for debugging purposes
            float left_channel_dbs = amplitude_to_db(max);
            data->left_channel_dbs = left_channel_dbs;

            // Smooth out values for the left channel so it doesn't jump around too much
            int target_left_volume_height = db_to_vu_height(left_channel_dbs, vumeter_border_height);
            current_left_volume_height = lerp(current_left_volume_height, target_left_volume_height, lerp_factor);
        }
        else if (c == 1)
        {
            // Store the decibels for debugging purposes
            float right_channel_dbs = amplitude_to_db(max);
            data->right_channel_dbs = right_channel_dbs;

            // Smooth out values for the right channel so it doesn't jump around too much
            int target_right_volume_height = db_to_vu_height(right_channel_dbs, vumeter_border_height);
            current_right_volume_height = lerp(current_right_volume_height, target_right_volume_height, lerp_factor);
        }
    }

    fflush(stdout);
    pw_stream_queue_buffer(data->stream, b);
}
/* [on_process] */

/* Be notified when the stream param changes. We're only looking at
 * the format changes.
 */
static void on_stream_param_changed(void *_data, uint32_t id, const struct spa_pod *param)
{
    struct pipewire_data *data = _data;

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
    struct pipewire_data* data = userdata;
    pw_main_loop_quit(data->loop);
    cleanup_ncurses();

    pw_stream_destroy(data->stream);
    pw_main_loop_destroy(data->loop);
    pw_deinit();

    exit(EXIT_SUCCESS);
}
/* PIPEWIRE */
