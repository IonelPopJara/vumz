/*
 * VUMZ (VU Meter visualiZer)
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

#define DEBUG_WIN_HEIGHT 8;
#define DEBUG_WIN_WIDTH 19;

#define CLAMP(val, min, max) (val < min ? min : (val > max ? max : val))

/* STRUCTS */
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

struct vumeter {
    int height;
    int width;
    int starty;
    int startx;
    int fill_height;
};

struct ncurses_window {
    WINDOW* win;
    int height;
    int width;
    int starty;
    int startx;
};
/* STRUCTS */

/* VARIABLES */
int green_threshold_height = 0;
int yellow_threshold_height = 0;
float smooth_factor = 1.0f;
/* VARIABLES */

void print_help();

/* UTIL */
static int db_to_vu_height(float db, int vu_height);
static float amplitude_to_db(float amplitude);
// The smooth fuction uses Exponential Moving Average (EMA) for now
static int smooth(int previous, int current, float factor);
/* UTIL */

/* NCURSES */
void init_ncurses();
WINDOW *create_newwin(int height, int width, int starty, int startx);
void destroy_win(WINDOW *local_win);
void draw_debug_info(WINDOW *local_win, void* vumeter_data);
void draw_vumeter_border(WINDOW* local_win, void* vumeter);
void draw_vumeter_data(WINDOW* local_win, int channel_dbs, void *vumeter);
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

    // Left vumeter
    struct vumeter left_vumeter = {0};
    left_vumeter.height = win_vumeter.height;
    left_vumeter.width = 2;
    left_vumeter.starty = 0;
    left_vumeter.startx = (win_vumeter.width / 2) - left_vumeter.width - 3;

    // Right vumeter
    struct vumeter right_vumeter = {0};
    right_vumeter.height = win_vumeter.height;
    right_vumeter.width = 2;
    right_vumeter.starty = 0;
    right_vumeter.startx = (win_vumeter.width / 2) + right_vumeter.width - 1;

    // Calculate the thresholds for the VU Meter
    green_threshold_height = db_to_vu_height(VUMETER_GREEN_THRESHOLD_DB, win_vumeter.height);
    yellow_threshold_height = db_to_vu_height(VUMETER_YELLOW_THRESHOLD_DB, win_vumeter.height);

    draw_vumeter_border(win_vumeter.win, &left_vumeter);        // Draw the left vumeter border
    draw_vumeter_border(win_vumeter.win, &right_vumeter);       // Draw the right vumeter border
    /* Ncurses config */

    while (true)
    {
        pw_loop_iterate(pw_main_loop_get_loop(data.loop), 0);

        if (debug_mode)
            draw_debug_info(win_debug.win, &data);

        draw_vumeter_data(win_vumeter.win, data.left_channel_dbs, &left_vumeter);
        draw_vumeter_data(win_vumeter.win, data.right_channel_dbs, &right_vumeter);

        if (screensaver_mode && getch() != ERR)
        {
            break;
        }
        else if (!screensaver_mode)
        {
            int c = getch();
            switch (c) {
                case KEY_UP:
                    smooth_factor += 0.05;
                    break;
                case KEY_DOWN:
                    smooth_factor -= 0.05;
                    break;
            }

            smooth_factor = CLAMP(smooth_factor, 0, 1);
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

static float amplitude_to_db(float amplitude)
{
    if (amplitude <= 0.0f)
    {
        return -60.0f;
    }

    return 20.0f * log10f(amplitude);
}

static int smooth(int previous, int current, float factor)
{
    return (int)(previous * (1.0f - factor) + current * factor);
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
    mvwprintw(local_win, 6, 2, "smooth: %.2f", smooth_factor);
    wattroff(local_win, COLOR_PAIR(5));
    wrefresh(local_win);
}

void draw_vumeter_border(WINDOW *local_win, void* vumeter)
{
    struct vumeter * current_vumeter = (struct vumeter *)vumeter;

    int height = current_vumeter->height;
    int width = current_vumeter->width;
    int starty = current_vumeter->starty;
    int startx = current_vumeter->startx;

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

void draw_vumeter_data(WINDOW* local_win, int channel_dbs, void *vumeter)
{
    struct vumeter *current_vumeter = (struct vumeter *)vumeter;
    int height = current_vumeter->height;
    int starty = current_vumeter->starty;
    int startx = current_vumeter->startx;

    int target_height = db_to_vu_height(channel_dbs, height);
    current_vumeter->fill_height = smooth(current_vumeter->fill_height, target_height, smooth_factor);
    int current_height = current_vumeter->fill_height;

    startx++;
    starty++;
    for (int i = 0; i < height - 2; i++)
    {
        // Let's say current_height is 10, which means that we need to draw 10 lines and height is 20
        // We start from the top of the vumeter, and we have to draw the bottom 10 lines with color
        // if i = 0, height - i = 20, 20 > 10 (current_height), no color
        // if i = 10, height - i = 10, 10 = 10 (current_height), color
        // if i = 11, height - i = 9, 9 < 10 (current_height), color
        // therefore, as long as height - i < current_height, we need to apply color
        // Apply color according to db
        if (height - i < current_height)
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
        }
        else if (c == 1)
        {
            // Store the decibels for debugging purposes
            float right_channel_dbs = amplitude_to_db(max);
            data->right_channel_dbs = right_channel_dbs;
        }
    }

    fflush(stdout);
    pw_stream_queue_buffer(data->stream, b);
}

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
