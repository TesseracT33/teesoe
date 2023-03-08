#pragma once

#include "types.hpp"

union SDL_Event;
class Status;

namespace frontend::audio {

void disable();
void enable();
Status init();
void on_device_added(SDL_Event event);
void on_device_removed(SDL_Event event);
void push_sample(s16 left, s16 right);
void set_sample_rate(u32 sample_rate);

} // namespace frontend::audio
