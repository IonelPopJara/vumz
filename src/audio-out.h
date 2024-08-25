#ifndef AUDIO_OUT_H
#define AUDIO_OUT_H

#include <ncurses.h>
#include "audio-cap.h"

void init_ncurses();
void draw_vumeter_data(const struct audio_data* audio);
void cleanup_ncurses();

#endif // AUDIO_OUT_H
