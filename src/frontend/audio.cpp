#include "audio.hpp"
#include "message.hpp"
#include "status.hpp"

#include "SDL.h"

#include <format>

namespace frontend::audio {

static SDL_AudioDeviceID audio_device_id;

void disable()
{
    // TODO
}

void enable()
{
    // TODO
}

Status init()
{
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        return status_failure(std::format("Failed call to SDL_Init: {}", SDL_GetError()));
    }

    static constexpr int default_sample_rate = 44100;
    static constexpr int default_num_output_channels = 2;
    static constexpr int default_sample_buffer_size_per_channel = 512;

    SDL_AudioSpec desired_spec = { .freq = default_sample_rate,
        .format = AUDIO_S16MSB,
        .channels = default_num_output_channels,
        .samples = default_sample_buffer_size_per_channel };

    SDL_AudioSpec obtained_spec;
    audio_device_id = SDL_OpenAudioDevice(nullptr, 0, &desired_spec, &obtained_spec, 0);
    if (audio_device_id == 0) {
        // message::warning(std::format("Could not open an audio device; {}", SDL_GetError()));
        // return false;
    }

    // SetSampleRate(obtained_spec.freq);
    // num_output_channels = obtained_spec.channels;
    // sample_buffer_size_per_channel = obtained_spec.samples;

    SDL_PauseAudioDevice(audio_device_id);

    return status_ok();
}

void on_device_added(SDL_Event event)
{
    // TODO
}

void on_device_removed(SDL_Event event)
{
    // TODO
}

} // namespace frontend::audio
