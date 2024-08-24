#ifndef AUDIO_OUT_H
#define AUDIO_OUT_H

#include <ncurses.h>

void init_ncurses();
void draw_vumeter_data(const float* audio_out_buffer, const float* audio_out_buffer_prev, int n_channels, double noise_reduction);
void cleanup_ncurses();

#endif // AUDIO_OUT_H
