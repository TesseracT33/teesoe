#include "audio.hpp"
#include "frontend/message.hpp"
#include "status.hpp"

#include "SDL3/SDL.h"

#include <format>

namespace frontend::audio {

static SDL_AudioDeviceID audio_device_id;

void Disable()
{
    // TODO
}

void Enable()
{
    // TODO
}

Status Init()
{
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        return FailureStatus("Failed call to SDL_Init: {}", SDL_GetError());
    }

    static constexpr int default_sample_rate = 44100;
    static constexpr int default_num_output_channels = 2;
    // static constexpr int default_sample_buffer_size_per_channel = 512;

    SDL_AudioSpec spec = {
        .format = SDL_AUDIO_S16BE,
        .channels = default_num_output_channels,
        .freq = default_sample_rate,
    };

    [[maybe_unused]] SDL_AudioSpec obtained_spec;
    audio_device_id = SDL_OpenAudioDevice(0, &spec);
    if (audio_device_id == 0) {
        // message::warning(std::format("Could not open an audio device; {}", SDL_GetError()));
        // return false;
    }

    // SetSampleRate(obtained_spec.freq);
    // num_output_channels = obtained_spec.channels;
    // sample_buffer_size_per_channel = obtained_spec.samples;

    SDL_PauseAudioDevice(audio_device_id);

    return OkStatus();
}

void OnDeviceAdded(SDL_Event event)
{
    (void)event;
    // TODO
}

void OnDeviceRemoved(SDL_Event event)
{
    (void)event;
    // TODO
}

void PushSample(s16 left, s16 right)
{
    (void)left;
    (void)right;
    // TODO
}

void SetSampleRate(u32 sample_rate)
{
    (void)sample_rate;
    // TODO
}

} // namespace frontend::audio
