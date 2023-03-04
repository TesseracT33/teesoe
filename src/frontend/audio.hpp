#pragma once

union SDL_Event;
class Status;

namespace frontend::audio {

void disable();
void enable();
Status init();
void on_device_added(SDL_Event event);
void on_device_removed(SDL_Event event);

} // namespace frontend::audio
