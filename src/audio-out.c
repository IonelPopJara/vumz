#include "audio-out.h"
#include <ncurses.h>
#include <stdlib.h>

#define VUMETER_GREEN_THRESHOLD_DB -25.0f
#define VUMETER_YELLOW_THRESHOLD_DB -10.0f

// 100%, 87.5%, 75%, 62.5%, 50%, 37.5%, 25%, 12.5%, 0%
/*static char* fill_percentage[8] = {"█", "▇", "▆", "▅", "▄", "▃", "▂", "▁"};*/
static char* fill_percentage[9] = {"+", "=", "=", "~", "-", "-", "_", "_", "·"};

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
        init_pair(6, 8, -1); // Grey?
    }

    refresh(); // Refresh the screen
}

//TODO: Optimize rendering
void draw_vumeter_data(const struct audio_data* audio) {
    // -- Get output buffers --
    const float* audio_out_buffer = audio->audio_out_buffer;
    const float* audio_out_buffer_prev = audio->audio_out_buffer_prev;
    // -- Get terminal dimmensions and calculate color threshold levels --
    int terminal_height, terminal_width;
    getmaxyx(stdscr, terminal_height, terminal_width);
    int green_threshold_height = db_to_vu_height(VUMETER_GREEN_THRESHOLD_DB, terminal_height);
    int yellow_threshold_height = db_to_vu_height(VUMETER_YELLOW_THRESHOLD_DB, terminal_height);

    // -- Calculate starting positions and other shenanigans --
    int vu_bar_width = terminal_width / 4;
    /*int vu_bar_width = 5;*/
    int startx_left = (terminal_width / 4) - (vu_bar_width / 2) + 3;
    int startx_right = (3 * terminal_width / 4) - (vu_bar_width / 2) - 3;

    // -- Get the current buffer data of the left channel --
    double current_left_height = db_to_vu_height(audio_out_buffer[0], terminal_height); // Current left channel height as a decimal
    int current_left_block_height = (int) current_left_height; // Current left channel height as an integer
    double left_percentage = get_fill_percentage(current_left_height); // Percentage of filling for the decimal part (e,g,. 2.75 => 75%)

    // -- Get the current buffer data of the right channel --
    double current_right_height = db_to_vu_height(audio_out_buffer[1], terminal_height); // Current right channel height as a decimal
    int current_right_block_height = (int) current_right_height; // Current right channel height as an integer
    double right_percentage = get_fill_percentage(current_right_height); // Percentage of filling for the decimal part

    // -- Get the previous buffer data of the left channel --
    double previous_left_height = db_to_vu_height(audio_out_buffer_prev[0], terminal_height); // Previous left channel height as a decimal
    int previous_left_block_height = (int) previous_left_height; // Previous left channel height as an integer

    // -- Get the previous buffer data of the right channel --
    double previous_right_height = db_to_vu_height(audio_out_buffer_prev[1], terminal_height); // Previous right channel height as a decimal
    int previous_right_block_height = (int) previous_right_height; // Previous right channel height as an integer

    // NOTE: Apparently, due to how my smooth function works, current height will never be greater than previous height (?) I don't get it
    bool isThisWorking = false;

    // Main render call
    attron(COLOR_PAIR(6)); // gray
    for (int i = 0; i < terminal_height - 1; i++) { 
        for (int j = 0; j < terminal_width; j++) {
            // If the current area is outside/in between the vu bars
            if ((j < startx_left || j >= startx_right + vu_bar_width) || (j >= startx_left + vu_bar_width && j < startx_right)) {
                attroff(A_COLOR);
                attron(COLOR_PAIR(6)); // gray
                mvprintw(i, j, "%s", fill_percentage[8]);
                attroff(A_COLOR);
            }
            // Else if we are inside the left vumeter
            else if (j >= startx_left && j < startx_left + vu_bar_width){
                attroff(A_COLOR);
                if (audio->color_theme <= 5) {
                    attron(COLOR_PAIR(audio->color_theme));
                }
                else {
                    // Use the green yellow red theme
                    // Calculate coordinates
                    if (terminal_height - i < green_threshold_height) {
                        attron(COLOR_PAIR(1));
                    }
                    else if (terminal_height - i < yellow_threshold_height) {
                        attron(COLOR_PAIR(2));
                    }
                    else {
                        attron(COLOR_PAIR(3));
                    }
                }

                if (terminal_height - i <= current_left_block_height) {
                    // Full block
                    mvprintw(i, j, "%s", fill_percentage[0]);
                }
                else if (terminal_height - i < current_left_height + 1 && terminal_height - i > current_left_height) {
                    // Percentage fill
                    int fill_index = get_fill_percentage_index(left_percentage);
                    mvprintw(i, j, "%s", fill_percentage[fill_index]);
                }
                else {
                    attroff(A_COLOR);
                    attron(COLOR_PAIR(6)); // gray
                    // Dot
                    mvprintw(i, j, "%s", fill_percentage[8]);
                    attroff(A_COLOR);
                }
                attroff(A_COLOR);
            }
            else if (j >= startx_right && j < startx_right + vu_bar_width) {

                attroff(A_COLOR);
                if (audio->color_theme <= 5) {
                    attron(COLOR_PAIR(audio->color_theme));
                }
                else {
                    // Use the green yellow red theme
                    // Calculate coordinates
                    if (terminal_height - i < green_threshold_height) {
                        attron(COLOR_PAIR(1));
                    }
                    else if (terminal_height - i < yellow_threshold_height) {
                        attron(COLOR_PAIR(2));
                    }
                    else {
                        attron(COLOR_PAIR(3));
                    }
                }

                if (terminal_height - i <= current_right_block_height) {
                    // Full block
                    mvprintw(i, j, "%s", fill_percentage[0]);
                }
                else if (terminal_height - i < current_right_height + 1 && terminal_height - i > current_right_height) {
                    // Percentage fill
                    int fill_index = get_fill_percentage_index(right_percentage);
                    mvprintw(i, j, "%s", fill_percentage[fill_index]);
                }
                else {
                    attroff(A_COLOR);
                    attron(COLOR_PAIR(6)); // gray
                    // Dot
                    mvprintw(i, j, "%s", fill_percentage[8]);
                    attroff(A_COLOR);
                }
                attroff(A_COLOR);
            }
        }
    }
    // Render bottom line
    for (int j = 0; j < terminal_width; j++) {
        // If the current area is outside/in between the vu bars
        if ((j < startx_left || j >= startx_right + vu_bar_width) || (j >= startx_left + vu_bar_width && j < startx_right)) {
            attroff(A_COLOR);
            attron(COLOR_PAIR(6)); // gray
            mvprintw(terminal_height - 1, j, "%s", fill_percentage[8]);
            attroff(A_COLOR);
        }
        // Else if we are inside the left vumeter
        else if (j >= startx_left && j < startx_left + vu_bar_width){
            attroff(A_COLOR);
            if (audio->color_theme <= 5) {
                attron(COLOR_PAIR(audio->color_theme));
            }
            else {
                // Use the green yellow red theme
                attron(COLOR_PAIR(1));
            }

            if (current_left_height <= 2)
            {
                mvprintw(terminal_height - 1, j, "%s", fill_percentage[2]);
            }
            else {
                mvprintw(terminal_height - 1, j, "%s", fill_percentage[0]);
            }
            attroff(A_COLOR);
        }
        else if (j >= startx_right && j < startx_right + vu_bar_width) {
            attroff(A_COLOR);
            if (audio->color_theme <= 5) {
                attron(COLOR_PAIR(audio->color_theme));
            }
            else {
                // Use the green yellow red theme
                attron(COLOR_PAIR(1));
            }
            if (current_right_height <= 2)
            {
                mvprintw(terminal_height - 1, j, "%s", fill_percentage[2]);
            }
            else {
                mvprintw(terminal_height - 1, j, "%s", fill_percentage[0]);
            }
            attroff(A_COLOR);
        }
    }
    attroff(A_COLOR);

    // Debug
    if (audio->debug == 1) {
        mvprintw(0, 0, "Color theme: %d", audio->color_theme);
        mvprintw(1, 0, "Noise reduction: %.2f", audio->noise_reduction);
	mvprintw(2, 0, "Sensitivity: %.2f", audio->sensitivity);
    }
    // Print to debug
    refresh();
}

void cleanup_ncurses()
{
    endwin();
    system("clear");
}

