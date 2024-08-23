/*
 * VUMZ (VU Meter visualiZer)
 */

#include <ncurses.h>

#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <pthread.h>
#include "audio-cap.h"
#include "audio-out.h"

#define DEBUG_MIN_HEIGHT 7
#define DEBUG_MIN_WIDTH 25

#define VUMETER_GREEN_THRESHOLD_DB -20.0f
#define VUMETER_YELLOW_THRESHOLD_DB -6.0f

#define DEBUG_WIN_HEIGHT 8;
#define DEBUG_WIN_WIDTH 19;

#define CLAMP(val, min, max) (val < min ? min : (val > max ? max : val))

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

/* VARIABLES */
int green_threshold_height = 0;
int yellow_threshold_height = 0;
float smooth_factor = 1.0f;
/* VARIABLES */

void print_help();

/* UTIL */
static int db_to_vu_height(float db, int vu_height);
// The smooth fuction uses Exponential Moving Average (EMA) for now
static int smooth(int previous, int current, float factor);
/* UTIL */

/* NCURSES */
void init_ncurses();
WINDOW *create_newwin(int height, int width, int starty, int startx);
void destroy_win(WINDOW *local_win);
/*void draw_debug_info(WINDOW *local_win, void* vumeter_data);*/
void draw_vumeter_border(WINDOW* local_win, void* vumeter);
void draw_vumeter_data(WINDOW* local_win, int channel_dbs, void *vumeter);
void cleanup_ncurses();
/* NCURSES */

int main(int argc, char **argv)
{
    pthread_t audio_thread;
    struct audio_data audio = {};

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

    setlocale(LC_ALL, ""); // Set locale so unicode characters work properly

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

    // Main loop
    while (true)
    {
        /*if (debug_mode)*/
            /*draw_debug_info(win_debug.win, &audio);*/

        draw_vumeter_data(win_vumeter.win, audio.left_channel_dbs, &left_vumeter);
        draw_vumeter_data(win_vumeter.win, audio.right_channel_dbs, &right_vumeter);

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

static int smooth(int previous, int current, float factor)
{
    return (int)(current * factor + previous * (1.0f - factor));
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

    if (has_colors())
    {
        use_default_colors(); // Allow using the default terminal colors
        init_pair(1, COLOR_GREEN, -1); // First color pair, this will be used for the lower volume levels
        init_pair(2, COLOR_YELLOW, -1); // Second color pair, this will be used for the medium volume levels
        init_pair(3, COLOR_RED, -1); // Third color pair, this will be used for the higher volume levels
        init_pair(4, COLOR_MAGENTA, -1); // Fourth color pair, this will be used for the debug window
        init_pair(5, COLOR_BLUE, -1); // Fifth color pair, this will be used for the debug window
    }
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

/*void draw_debug_info(WINDOW *local_win, void *vumeter_data)*/
/*{*/
/*    struct pipewire_data *data = (struct pipewire_data *)vumeter_data;*/
/*    wattron(local_win, COLOR_PAIR(4));*/
/*    mvwprintw(local_win, 1, 2, "VUMZ");*/
/*    mvwprintw(local_win, 2, 2, "---------------");*/
/*    wattroff(local_win, COLOR_PAIR(4));*/
/*    wattron(local_win, COLOR_PAIR(5));*/
/*    mvwprintw(local_win, 3, 2, "Channels: %d", data->n_channels);*/
/*    mvwprintw(local_win, 4, 2, "|- L: %.2f dB", data->left_channel_dbs);*/
/*    mvwprintw(local_win, 5, 2, "|- R: %.2f dB", data->right_channel_dbs);*/
/*    mvwprintw(local_win, 6, 2, "smooth: %.2f", smooth_factor);*/
/*    wattroff(local_win, COLOR_PAIR(5));*/
/*    wrefresh(local_win);*/
/*}*/

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

