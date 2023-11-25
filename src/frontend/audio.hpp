#pragma once

#include "types.hpp"

union SDL_Event;
class Status;

namespace frontend::audio {

void Disable();
void Enable();
Status Init();
void OnDeviceAdded(SDL_Event event);
void OnDeviceRemoved(SDL_Event event);
void PushSample(s16 left, s16 right);
void SetSampleRate(u32 sample_rate);

} // namespace frontend::audio
