#include "audio-out.h"
#include <ncurses.h>
#include <stdlib.h>

#define VUMETER_GREEN_THRESHOLD_DB -20.0f
#define VUMETER_YELLOW_THRESHOLD_DB -6.0f

// 100%, 87.5%, 75%, 62.5%, 50%, 37.5%, 25%, 12.5%, 0%
static char* fill_percentage[8] = {"█", "▇", "▆", "▅", "▄", "▃", "▂", "▁"};

static double db_to_vu_height(float db, int vu_height)
{
    // height = 39 when my terminal is using the whole height
    // db = 0 to test
    if (db <= -60.0f)
    {
        db = -60.0f;
    }

    if (db >= 0.0f)
    {
        db = 0.0f;
    }

    return (double)(((db + 60.0f) / 60.0f) * vu_height);
}

static int get_fill_percentage_index(double percentage) {
    int fill_index;
    if (percentage >= 87.5) {
        // 100 >= x >= 87.5 -> █
        fill_index = 0;

    } else if (percentage >= 75) {
        // 87.5 > x >= 75 -> ▇
        fill_index = 1;

    } else if (percentage >= 62.5) {
        // 75 > x >= 62.5 -> ▆
        fill_index = 2;

    } else if (percentage >= 50) {
        // 62.5 > x >= 50 -> ▅
        fill_index = 3;

    } else if (percentage >= 37.5) {
        // 50 > x >= 37.5 -> ▄
        fill_index = 4;

    } else if (percentage >= 25) {
        // 37.5 > x >= 25 -> ▃
        fill_index = 5;

    } else if (percentage >= 12.5) {
        // 25 > x >= 12.5 -> ▂
        fill_index = 6;
    } else {
        // 12.5 > x > 0 -> ▁
        fill_index = 7;
    }

    return fill_index;
}

double get_fill_percentage(double input) {
    // Calculate the fractional part of a number (e,g,. 8.623 -> 62.3)
    double percentage = (input - (int)input) * 100;

    return percentage;
}

void init_ncurses()
{
    initscr();              /* Start curses mode */
    raw();                  /* Line buffering disabled */
    keypad(stdscr, TRUE);   /* Enable keypad */
    noecho();               /* Don't echo user input */
    curs_set(0);            /* Make the cursor invisible */
    timeout(0);             /* Non blocking mode */
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

    int height, width;
    getmaxyx(stdscr, height, width);

    mvprintw(0, 0, "Height: %d | Width %d\n", height, width);

    refresh(); // Refresh the screen
    // TODO: call getmaxyx
}

void draw_vumeter_data(float left_channel_dbs, float right_channel_dbs)
{
    int terminal_height, terminal_width;
    getmaxyx(stdscr, terminal_height, terminal_width);

    int vu_bar_width = 4;

    int startx = terminal_width / 2;

    double current_height = db_to_vu_height(left_channel_dbs, terminal_height);
    int green_threshold_height = db_to_vu_height(VUMETER_GREEN_THRESHOLD_DB, terminal_height);
    int yellow_threshold_height = db_to_vu_height(VUMETER_YELLOW_THRESHOLD_DB, terminal_height);

    mvprintw(2, 0, "%.2f DBs", left_channel_dbs);
    mvprintw(3, 0, "Current Height: %f", current_height);
    mvprintw(4, 0, "Int height: %d", (int)current_height);
    mvprintw(terminal_height - 1, 0, "Terminal height: %d", terminal_height);
    double percentage = get_fill_percentage(current_height);
    mvprintw(5, 0, "Percentage:%.2f ", percentage);

    // Main render
    for (int i = 0; i < terminal_height; i++)
    {
        // Apply color according to db
        if (terminal_height - i < green_threshold_height) {
            //green
            attron(COLOR_PAIR(1));
        }
        else if (terminal_height - i < yellow_threshold_height) {
            // yellow
            attron(COLOR_PAIR(2));
        }
        else {
            // red
            attron(COLOR_PAIR(3));
        }

        if (terminal_height - i <= current_height) {

            // Draw a colored block;
            for (int j = 0; j < vu_bar_width; j++)
            {
                // check how tall the block should be
                mvprintw(0 + i, startx + j, "█");
            }

        }
        else if (terminal_height - i > current_height && terminal_height - i < current_height + 1) {
            // If it is the top block
            // Check the percentage
            int fill_index = get_fill_percentage_index(percentage);

            for (int j = 0; j < vu_bar_width; j++)
            {
                mvprintw(0 + i, startx + j, "%s", fill_percentage[fill_index]);
            }
        }
        else {
            // If it's none of the above
            for (int j = 0; j < vu_bar_width; j++)
            {
                mvprintw(0 + i, startx + j, " ");
            }
        }

        // Deactivate color attributes
        attroff(A_COLOR);
    }
    refresh();
}

void cleanup_ncurses()
{
    endwin();
    system("clear");
}
