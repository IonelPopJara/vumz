#ifndef AUDIO_OUT_H
#define AUDIO_OUT_H

#include <ncurses.h>
#include <stdlib.h>

void init_ncurses();
void draw_vumeter_data(float left_channel_dbs, float right_channel_dbs);
void cleanup_ncurses();

#endif // AUDIO_OUT_H
