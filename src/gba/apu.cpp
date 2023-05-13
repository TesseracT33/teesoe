#include "apu.hpp"
#include "bus.hpp"

#include <array>

namespace gba::apu {

enum class Direction {
    Decreasing,
    Increasing /* Sweep and envelope */
};

struct Channel {
protected:
    explicit Channel(uint id) : id(id){};

public:
    void Disable();
    void Enable();

    const uint id;

    bool dac_enabled;
    bool enabled;
    uint freq;
    uint timer;
    uint volume;
    f32 output;
};

struct Envelope {
    explicit Envelope(Channel* ch) : ch(ch) {}

    void Clock();
    void Enable();
    void Initialize();
    void SetParams(u8 data);

    bool is_updating;
    uint initial_volume;
    uint period;
    uint timer;
    Direction direction;
    Channel* const ch;
};

struct LengthCounter {
    explicit LengthCounter(Channel* ch) : ch(ch) {}

    void Clock();
    void Initialize();

    bool enabled;
    uint value;
    uint length;
    Channel* const ch;
};

struct Sweep {
    explicit Sweep(Channel* ch) : ch(ch) {}

    void Clock();
    uint ComputeNewFreq();
    void Enable();
    void Initialize();

    bool enabled;
    bool negate_has_been_used;
    uint period;
    uint shadow_freq;
    uint shift;
    uint timer;
    Direction direction;
    Channel* const ch;
};

template<bool has_sweep> struct PulseChannel : Channel {
    PulseChannel(uint id) : Channel(id) {}

    f32 GetOutput();
    void Initialize();
    void Step();
    void Trigger();

    uint duty;
    uint wave_pos;
    Envelope envelope{ this };
    LengthCounter length_counter{ this };
    Sweep sweep{ this };
};

static PulseChannel<true> pulse_ch_1{ 0 };
static PulseChannel<false> pulse_ch_2{ 1 };

struct WaveChannel : Channel {
    WaveChannel() : Channel(2) {}

    f32 GetOutput();
    void Initialize();
    void Step();
    void Trigger();

    uint output_level;
    uint wave_pos;
    u8 sample_buffer;
    LengthCounter length_counter{ this };
} static wave_ch;

struct NoiseChannel : Channel {
    NoiseChannel() : Channel(3) {}

    f32 GetOutput();
    void Initialize();
    void Step();
    void Trigger();

    u16 lfsr;
    Envelope envelope{ this };
    LengthCounter length_counter{ this };
} static noise_ch;

static void DisableAPU();
static void EnableAPU();
static void ResetAllRegisters();
static void Sample();

static bool apu_enabled;

static u8 nr10, nr11, nr12, nr13, nr14, nr21, nr22, nr23, nr24, nr30, nr31, nr32, nr33, nr34, nr41, nr42, nr43, nr44,
  nr50, nr51, nr52;

static uint frame_seq_step_counter;
static uint sample_rate;
static uint t_cycle_sample_counter;

static std::array<u8, 0x10> wave_ram;

void ApplyNewSampleRate()
{
    // sample_rate = Audio::GetSampleRate();
    t_cycle_sample_counter = 0;
}

void DisableAPU()
{
    ResetAllRegisters();
    apu_enabled = false;
    pulse_ch_1.wave_pos = pulse_ch_2.wave_pos = 0;
    wave_ch.sample_buffer = 0;
    frame_seq_step_counter = 0;
}

void EnableAPU()
{
    frame_seq_step_counter = 0;
    apu_enabled = true;
}

void Initialize()
{
    ResetAllRegisters();
    pulse_ch_1.Initialize();
    pulse_ch_2.Initialize();
    wave_ch.Initialize();
    noise_ch.Initialize();
    // wave_ram_accessible_by_cpu_when_ch3_enabled = true;
    frame_seq_step_counter = 0;
    ApplyNewSampleRate();
}

void ResetAllRegisters()
{
    WriteReg(bus::ADDR_NR10, 0);
    WriteReg(bus::ADDR_NR11, 0);
    WriteReg(bus::ADDR_NR12, 0);
    WriteReg(bus::ADDR_NR13, 0);
    WriteReg(bus::ADDR_NR14, 0);
    WriteReg(bus::ADDR_NR21, 0);
    WriteReg(bus::ADDR_NR22, 0);
    WriteReg(bus::ADDR_NR23, 0);
    WriteReg(bus::ADDR_NR24, 0);
    WriteReg(bus::ADDR_NR30, 0);
    WriteReg(bus::ADDR_NR31, 0);
    WriteReg(bus::ADDR_NR32, 0);
    WriteReg(bus::ADDR_NR33, 0);
    WriteReg(bus::ADDR_NR34, 0);
    WriteReg(bus::ADDR_NR41, 0);
    WriteReg(bus::ADDR_NR42, 0);
    WriteReg(bus::ADDR_NR43, 0);
    WriteReg(bus::ADDR_NR44, 0);
    WriteReg(bus::ADDR_NR50, 0);
    WriteReg(bus::ADDR_NR51, 0);
    nr52 = 0; /* Do not use WriteReg, as it will make a call to ResetAllRegisters again when 0 is written to nr52. */
}

void Sample()
{
    f32 right_output = 0.0f, left_output = 0.0f;
    if (nr51 & 0x11) {
        f32 pulse_ch_1_output = pulse_ch_1.GetOutput();
        right_output += pulse_ch_1_output * (nr51 & 1);
        left_output += pulse_ch_1_output * (nr51 >> 4 & 1);
    }
    if (nr51 & 0x22) {
        f32 pulse_ch_2_output = pulse_ch_2.GetOutput();
        right_output += pulse_ch_2_output * (nr51 >> 1 & 1);
        left_output += pulse_ch_2_output * (nr51 >> 5 & 1);
    }
    if (nr51 & 0x44) {
        f32 wave_ch_output = wave_ch.GetOutput();
        right_output += wave_ch_output * (nr51 >> 2 & 1);
        left_output += wave_ch_output * (nr51 >> 6 & 1);
    }
    if (nr51 & 0x88) {
        f32 noise_ch_output = noise_ch.GetOutput();
        right_output += noise_ch_output * (nr51 >> 3 & 1);
        left_output += noise_ch_output * (nr51 >> 7 & 1);
    }
    auto right_vol = nr50 & 7;
    auto left_vol = nr50 >> 4 & 7;
    f32 left_sample = left_vol / 28.0f * left_output;
    f32 right_sample = right_vol / 28.0f * right_output;
    // Audio::EnqueueSample(left_sample);
    // Audio::EnqueueSample(right_sample);
}

void StepFrameSequencer()
{
    // note: this function is called from the Timer module as DIV increases
    if (frame_seq_step_counter % 2 == 0) {
        pulse_ch_1.length_counter.Clock();
        pulse_ch_2.length_counter.Clock();
        wave_ch.length_counter.Clock();
        noise_ch.length_counter.Clock();
        if (frame_seq_step_counter % 4 == 2) {
            pulse_ch_1.sweep.Clock();
        }
    } else if (frame_seq_step_counter == 7) {
        pulse_ch_1.envelope.Clock();
        pulse_ch_2.envelope.Clock();
        noise_ch.envelope.Clock();
    }
    frame_seq_step_counter = (frame_seq_step_counter + 1) & 7;
}

void StreamState(Serializer& stream)
{
    /* TODO */
}

template<std::integral Int> Int ReadReg(u32 addr)
{
    auto ReadByte = [](u32 addr) {
        switch (addr) {
        case bus::ADDR_NR10: return u8(nr10 | 0x80);
        case bus::ADDR_NR11: return u8(nr11 | 0x3F);
        case bus::ADDR_NR12: return nr12;
        case bus::ADDR_NR13: return nr13;
        case bus::ADDR_NR14: return u8(nr14 | 0xBF);
        case bus::ADDR_NR21: return u8(nr21 | 0x3F);
        case bus::ADDR_NR22: return nr22;
        case bus::ADDR_NR23: return nr23;
        case bus::ADDR_NR24: return u8(nr24 | 0xBF);
        case bus::ADDR_NR30: return u8(nr30 | 0x7F);
        case bus::ADDR_NR31: return nr31;
        case bus::ADDR_NR32: return u8(nr32 | 0x9F);
        case bus::ADDR_NR33: return nr33;
        case bus::ADDR_NR34: return u8(nr34 | 0xBF);
        case bus::ADDR_NR41: return nr41;
        case bus::ADDR_NR42: return nr42;
        case bus::ADDR_NR43: return nr43;
        case bus::ADDR_NR44: return u8(nr44 | 0xBF);
        case bus::ADDR_NR50: return nr50;
        case bus::ADDR_NR51: return nr51;
        case bus::ADDR_DMA_SOUND_CTRL: return u8(0); // TODO
        case bus::ADDR_NR52: return u8(nr52 | 0x70);
        case bus::ADDR_SOUNDBIAS: return u8(0); // TODO
        case bus::ADDR_FIFO_A: return u8(0); // TODO
        case bus::ADDR_FIFO_B:
            return u8(0); // TODO
            // TODO; wave ram
        default: return bus::ReadOpenBus<u8>(addr);
        }
    };

    auto ReadHalf = [](u32 addr) {
        switch (addr) {
        case bus::ADDR_NR10: return u16(nr10 | 0x80);
        case bus::ADDR_NR11: return u16(nr11 | nr12 | 0xFF3F);
        case bus::ADDR_NR13: return u16(nr13 | nr14 | 0xBFFF);
        case bus::ADDR_NR21: return u16(nr21 | nr22 | 0xFF3F);
        case bus::ADDR_NR23: return u16(nr23 | nr24 | 0xBFFF);
        case bus::ADDR_NR30: return u16(nr30 | 0x7F);
        case bus::ADDR_NR31: return u16(nr31 | nr32 | 0x9FFF);
        case bus::ADDR_NR33: return u16(nr33 | nr34 | 0xBFFF);
        case bus::ADDR_NR41: return u16(nr41 | nr42);
        case bus::ADDR_NR43: return u16(nr43 | nr44 | 0xBFFF);
        case bus::ADDR_NR50: return u16(nr50 | nr51);
        case bus::ADDR_DMA_SOUND_CTRL: return u16(0); // TODO
        case bus::ADDR_NR52: return u16(nr52 | 0x70);
        case bus::ADDR_SOUNDBIAS: return u16(0); // TODO
        case bus::ADDR_FIFO_A: return u16(0); // TODO
        case bus::ADDR_FIFO_B: return u16(0); // TODO
        default: return bus::ReadOpenBus<u16>(addr);
        }
    };

    if constexpr (sizeof(Int) == 1) {
        return ReadByte(addr);
    }
    if constexpr (sizeof(Int) == 2) {
        return ReadHalf(addr);
    }
    if constexpr (sizeof(Int) == 4) {
        u16 lo = ReadHalf(addr);
        u16 hi = ReadHalf(addr + 2);
        return lo | hi << 16;
    }
}

// u8 ReadWaveRamCpu(u16 addr)
//{
//	addr &= 0xF;
//	// If the wave channel is enabled, accessing any byte from $FF30-$FF3F
//	// is equivalent to accessing the current byte selected by the waveform position.
//	// https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware
//	if (apu_enabled && wave_ch.enabled) {
//		if (wave_ram_accessible_by_cpu_when_ch3_enabled) {
//			return wave_ram[wave_ch.wave_pos / 2]; /* each byte encodes two samples */
//		}
//		else {
//			return 0xFF;
//		}
//	}
//	else {
//		return wave_ram[addr];
//	}
// }

// void WriteWaveRamCpu(u16 addr, u8 data)
//{
//	addr &= 0xF;
//	if (apu_enabled && wave_ch.enabled) {
//		if (wave_ram_accessible_by_cpu_when_ch3_enabled) {
//			wave_ram[wave_ch.wave_pos / 2] = data; /* each byte encodes two samples */
//		}
//	}
//	else {
//		wave_ram[addr] = data;
//	}
// }

// void Update()
//{
//	// Update() is called each m-cycle, but apu is updated each t-cycle
//	if (apu_enabled) {
//		for (int i = 0; i < 4; ++i) {
//			t_cycle_sample_counter += sample_rate;
//			if (t_cycle_sample_counter >= System::t_cycles_per_sec_base) {
//				Sample();
//				t_cycle_sample_counter -= System::t_cycles_per_sec_base;
//			}
//			pulse_ch_1.Step();
//			pulse_ch_1.Step();
//			wave_ch.Step();
//			noise_ch.Step();
//			// note: the frame sequencer is updated from the Timer module
//		}
//	}
//	else {
//		t_cycle_sample_counter += 4 * sample_rate;
//		if (t_cycle_sample_counter >= System::t_cycles_per_sec_base) {
//			Sample();
//			t_cycle_sample_counter -= System::t_cycles_per_sec_base;
//		}
//	}
// }

template<std::integral Int> void WriteReg(u32 addr, Int data)
{
    auto WriteNR10 = [&](u8 data) {
        nr10 = data;
        pulse_ch_1.sweep.period = nr10 >> 4 & 7;
        pulse_ch_1.sweep.direction = nr10 & 8 ? Direction::Decreasing : Direction::Increasing;
        pulse_ch_1.sweep.shift = nr10 & 7;
        if (pulse_ch_1.sweep.direction == Direction::Increasing && pulse_ch_1.sweep.negate_has_been_used) {
            pulse_ch_1.Disable();
        }
    };

    auto WriteNR11 = [&](u8 data) {
        nr11 = data;
        pulse_ch_1.duty = data >> 6;
        pulse_ch_1.length_counter.length = data & 0x3F;
        pulse_ch_1.length_counter.value = 64 - pulse_ch_1.length_counter.length;
    };

    auto WriteNR12 = [&](u8 data) {
        nr12 = data;
        pulse_ch_1.envelope.SetParams(data);
        pulse_ch_1.dac_enabled = data & 0xF8;
        if (!pulse_ch_1.dac_enabled) {
            pulse_ch_1.Disable();
        }
    };

    auto WriteNR13 = [&](u8 data) {
        nr13 = data;
        pulse_ch_1.freq &= 0x700;
        pulse_ch_1.freq |= data;
    };

    auto WriteNR14 = [&](u8 data) {
        nr14 = data;
        pulse_ch_1.freq &= 0xFF;
        pulse_ch_1.freq |= (data & 7) << 8;
        // Extra length clocking occurs when writing to NRx4 when the frame sequencer's next step
        // is one that doesn't clock the length counter. In this case, if the length counter was
        // PREVIOUSLY disabled and now enabled and the length counter is not zero, it is decremented.
        // If this decrement makes it zero and trigger is clear, the channel is disabled.
        // https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware#Obscure_Behavior
        bool enable_length = data & 0x40;
        bool trigger = data & 0x80;
        if (enable_length && !pulse_ch_1.length_counter.enabled && pulse_ch_1.length_counter.value > 0
            && frame_seq_step_counter & 1 && !trigger) {
            if (--pulse_ch_1.length_counter.value == 0) {
                pulse_ch_1.Disable();
            }
        }
        pulse_ch_1.length_counter.enabled = enable_length;
        if (trigger) {
            pulse_ch_1.Trigger();
        }
    };

    auto WriteNR21 = [&](u8 data) {
        nr21 = data;
        pulse_ch_2.duty = data >> 6;
        pulse_ch_2.length_counter.length = data & 0x3F;
        pulse_ch_2.length_counter.value = 64 - pulse_ch_2.length_counter.length;
    };

    auto WriteNR22 = [&](u8 data) {
        nr22 = data;
        pulse_ch_2.envelope.SetParams(data);
        pulse_ch_2.dac_enabled = data & 0xF8;
        if (!pulse_ch_2.dac_enabled) {
            pulse_ch_2.Disable();
        }
    };

    auto WriteNR23 = [&](u8 data) {
        nr23 = data;
        pulse_ch_2.freq &= 0x700;
        pulse_ch_2.freq |= data;
    };

    auto WriteNR24 = [&](u8 data) {
        nr24 = data;
        pulse_ch_2.freq &= 0xFF;
        pulse_ch_2.freq |= (data & 7) << 8;
        bool enable_length = data & 0x40;
        bool trigger = data & 0x80;
        if (enable_length && !pulse_ch_2.length_counter.enabled && pulse_ch_2.length_counter.value > 0
            && frame_seq_step_counter & 1 && !trigger) {
            if (--pulse_ch_2.length_counter.value == 0) {
                pulse_ch_2.Disable();
            }
        }
        pulse_ch_2.length_counter.enabled = enable_length;
        if (trigger) {
            pulse_ch_2.Trigger();
        }
    };

    auto WriteNR30 = [&](u8 data) {
        nr30 = data;
        wave_ch.dac_enabled = data & 0x80;
        if (!wave_ch.dac_enabled) {
            wave_ch.Disable();
        }
    };

    auto WriteNR31 = [&](u8 data) {
        nr31 = data;
        wave_ch.length_counter.length = data & 0x3F;
        wave_ch.length_counter.value = 256 - wave_ch.length_counter.length;
    };

    auto WriteNR32 = [&](u8 data) {
        nr32 = data;
        wave_ch.output_level = data >> 5 & 3;
    };

    auto WriteNR33 = [&](u8 data) {
        nr33 = data;
        wave_ch.freq &= 0x700;
        wave_ch.freq |= data;
    };

    auto WriteNR34 = [&](u8 data) {
        nr34 = data;
        wave_ch.freq &= 0xFF;
        wave_ch.freq |= (data & 7) << 8;
        bool enable_length = data & 0x40;
        bool trigger = data & 0x80;
        if (enable_length && !wave_ch.length_counter.enabled && wave_ch.length_counter.value > 0
            && frame_seq_step_counter & 1 && !trigger) {
            if (--wave_ch.length_counter.value == 0) {
                wave_ch.Disable();
            }
        }
        wave_ch.length_counter.enabled = enable_length;
        if (trigger) {
            wave_ch.Trigger();
        }
    };

    auto WriteNR41 = [&](u8 data) {
        nr41 = data;
        noise_ch.length_counter.length = data & 0x3F;
        noise_ch.length_counter.value = 64 - noise_ch.length_counter.length;
    };

    auto WriteNR42 = [&](u8 data) {
        nr42 = data;
        noise_ch.envelope.SetParams(data);
        noise_ch.dac_enabled = data & 0xF8;
        if (!noise_ch.dac_enabled) {
            noise_ch.Disable();
        }
    };

    auto WriteNR43 = [&](u8 data) { nr43 = data; };

    auto WriteNR44 = [&](u8 data) {
        nr44 = data;
        bool enable_length = data & 0x40;
        bool trigger = data & 0x80;
        if (enable_length && !noise_ch.length_counter.enabled && noise_ch.length_counter.value > 0
            && frame_seq_step_counter & 1 && !trigger) {
            if (--noise_ch.length_counter.value == 0) {
                noise_ch.Disable();
            }
        }
        noise_ch.length_counter.enabled = enable_length;
        if (trigger) {
            noise_ch.Trigger();
        }
    };

    auto WriteNR50 = [&](u8 data) { nr50 = data; };

    auto WriteNR51 = [&](u8 data) { nr51 = data; };

    auto WriteNR52 = [&](u8 data) {
        // If bit 7 is reset, then all of the sound system is immediately shut off, and all audio regs are cleared
        nr52 = data;
        nr52 & 0x80 ? EnableAPU() : DisableAPU();
    };

    auto WriteByte = [&](u32 addr, u8 data) {
        switch (addr) {
        case bus::ADDR_NR10: WriteNR10(data); break;
        case bus::ADDR_NR11: WriteNR11(data); break;
        case bus::ADDR_NR12: WriteNR12(data); break;
        case bus::ADDR_NR13: WriteNR13(data); break;
        case bus::ADDR_NR14: WriteNR14(data); break;
        case bus::ADDR_NR21: WriteNR21(data); break;
        case bus::ADDR_NR22: WriteNR22(data); break;
        case bus::ADDR_NR23: WriteNR23(data); break;
        case bus::ADDR_NR24: WriteNR24(data); break;
        case bus::ADDR_NR30: WriteNR30(data); break;
        case bus::ADDR_NR31: WriteNR31(data); break;
        case bus::ADDR_NR32: WriteNR32(data); break;
        case bus::ADDR_NR33: WriteNR33(data); break;
        case bus::ADDR_NR34: WriteNR34(data); break;
        case bus::ADDR_NR41: WriteNR41(data); break;
        case bus::ADDR_NR42: WriteNR42(data); break;
        case bus::ADDR_NR43: WriteNR43(data); break;
        case bus::ADDR_NR44: WriteNR44(data); break;
        case bus::ADDR_NR50: WriteNR50(data); break;
        case bus::ADDR_NR51: WriteNR51(data); break;
        case bus::ADDR_DMA_SOUND_CTRL: break; // TODO
        case bus::ADDR_NR52: WriteNR52(data); break;
        case bus::ADDR_SOUNDBIAS: break; // TODO
        case bus::ADDR_FIFO_A: break; // TODO
        case bus::ADDR_FIFO_B: break; // TODO
        case bus::ADDR_WAVE_RAM:
        case bus::ADDR_WAVE_RAM + 1:
        case bus::ADDR_WAVE_RAM + 2:
        case bus::ADDR_WAVE_RAM + 3:
        case bus::ADDR_WAVE_RAM + 4:
        case bus::ADDR_WAVE_RAM + 5:
        case bus::ADDR_WAVE_RAM + 6:
        case bus::ADDR_WAVE_RAM + 7:
        case bus::ADDR_WAVE_RAM + 8:
        case bus::ADDR_WAVE_RAM + 9:
        case bus::ADDR_WAVE_RAM + 10:
        case bus::ADDR_WAVE_RAM + 11:
        case bus::ADDR_WAVE_RAM + 12:
        case bus::ADDR_WAVE_RAM + 13:
        case bus::ADDR_WAVE_RAM + 14:
        case bus::ADDR_WAVE_RAM + 15: break;
        }
    };

    auto WriteHalf = [&](u32 addr, u16 data) {
        switch (addr) {
        case bus::ADDR_NR10: WriteNR10(data & 0xFF); break;
        case bus::ADDR_NR11:
            WriteNR11(data & 0xFF);
            WriteNR12(data >> 8 & 0xFF);
            break;
        case bus::ADDR_NR13:
            WriteNR13(data & 0xFF);
            WriteNR14(data >> 8 & 0xFF);
            break;
        case bus::ADDR_NR21:
            WriteNR21(data & 0xFF);
            WriteNR22(data >> 8 & 0xFF);
            break;
        case bus::ADDR_NR23:
            WriteNR23(data & 0xFF);
            WriteNR24(data >> 8 & 0xFF);
            break;
        case bus::ADDR_NR30: WriteNR30(data & 0xFF); break;
        case bus::ADDR_NR31:
            WriteNR31(data & 0xFF);
            WriteNR32(data >> 8 & 0xFF);
            break;
        case bus::ADDR_NR33:
            WriteNR33(data & 0xFF);
            WriteNR34(data >> 8 & 0xFF);
            break;
        case bus::ADDR_NR41:
            WriteNR41(data & 0xFF);
            WriteNR42(data >> 8 & 0xFF);
            break;
        case bus::ADDR_NR43:
            WriteNR43(data & 0xFF);
            WriteNR44(data >> 8 & 0xFF);
            break;
        case bus::ADDR_NR50:
            WriteNR50(data & 0xFF);
            WriteNR51(data >> 8 & 0xFF);
            break;
        case bus::ADDR_DMA_SOUND_CTRL: break; // TODO
        case bus::ADDR_NR52: WriteNR52(data & 0xFF); break;
        case bus::ADDR_SOUNDBIAS: break; // TODO
        case bus::ADDR_FIFO_A: break; // TODO
        case bus::ADDR_FIFO_B: break; // TODO
        case bus::ADDR_WAVE_RAM:
        case bus::ADDR_WAVE_RAM + 2:
        case bus::ADDR_WAVE_RAM + 4:
        case bus::ADDR_WAVE_RAM + 6:
        case bus::ADDR_WAVE_RAM + 8:
        case bus::ADDR_WAVE_RAM + 10:
        case bus::ADDR_WAVE_RAM + 12:
        case bus::ADDR_WAVE_RAM + 14: break;
        }
    };

    if (apu_enabled) {
        if constexpr (sizeof(Int) == 1) {
            WriteByte(addr, data);
        }
        if constexpr (sizeof(Int) == 2) {
            WriteHalf(addr, data);
        }
        if constexpr (sizeof(Int) == 4) {
            WriteHalf(addr, data & 0xFFFF);
            WriteHalf(addr + 2, data >> 16 & 0xFFFF);
        }
    } else {
        // TODO
    }
}

void Channel::Disable()
{
    nr52 &= ~(1 << id);
    enabled = false;
}

void Channel::Enable()
{
    nr52 |= 1 << id;
    enabled = true;
}

void Envelope::Clock()
{
    if (period != 0) {
        if (timer > 0) {
            timer--;
        }
        if (timer == 0) {
            timer = period > 0 ? period : 8;
            if (ch->volume < 0xF && direction == Direction::Increasing) {
                ch->volume++;
            } else if (ch->volume > 0x0 && direction == Direction::Decreasing) {
                ch->volume--;
            } else {
                is_updating = false;
            }
        }
    }
}

void Envelope::Enable()
{
    timer = period;
    is_updating = true;
    ch->volume = initial_volume;
}

void Envelope::Initialize()
{
    is_updating = false;
    initial_volume = period = timer = 0;
    direction = Direction::Decreasing;
}

void Envelope::SetParams(u8 data)
{
    // "Zombie" mode
    // https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware#Obscure_Behavior
    Direction new_direction = data & 8 ? Direction::Increasing : Direction::Decreasing;
    if (ch->enabled) {
        if (period == 0 && is_updating || direction == Direction::Decreasing) {
            ch->volume++;
        }
        if (new_direction != direction) {
            ch->volume = 0x10 - ch->volume;
        }
        ch->volume &= 0xF;
    }
    initial_volume = data >> 4;
    direction = new_direction;
    period = data & 7;
}

void LengthCounter::Clock()
{
    if (enabled && value > 0) {
        if (--value == 0) {
            ch->Disable();
        }
    }
}

void LengthCounter::Initialize()
{
    enabled = false;
    value = length = 0;
}

f32 NoiseChannel::GetOutput()
{
    return enabled * dac_enabled * volume * (~lfsr & 1) / 7.5f - 1.0f;
}

void NoiseChannel::Initialize()
{
    dac_enabled = enabled = false;
    volume = 0;
    output = 0.0f;
    lfsr = 0x7FFF;
    envelope.Initialize();
    length_counter.Initialize();
}

void NoiseChannel::Step()
{
    if (timer == 0) {
        static constexpr std::array divisor_table = { 8, 16, 32, 48, 64, 80, 96, 112 };
        auto divisor_code = nr43 & 7;
        auto clock_shift = nr43 >> 4;
        timer = divisor_table[divisor_code] << clock_shift;
        bool xor_result = (lfsr & 1) ^ (lfsr >> 1 & 1);
        lfsr = lfsr >> 1 | xor_result << 14;
        if (nr43 & 8) {
            lfsr &= ~(1 << 6);
            lfsr |= xor_result << 6;
        }
    } else {
        --timer;
    }
}

void NoiseChannel::Trigger()
{
    if (dac_enabled) {
        Enable();
    }
    envelope.Enable();
    if (length_counter.value == 0) {
        length_counter.value = 64;
        if (length_counter.enabled && frame_seq_step_counter <= 3) {
            --length_counter.value;
        }
    }
    // Reload period.
    // The low two bits of the frequency timer are NOT modified.
    // https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware
    timer = timer & 3 | freq & ~3;
    envelope.timer = envelope.period > 0 ? envelope.period : 8;
    // If a channel is triggered when the frame sequencer's next step will clock the volume envelope,
    // the envelope's timer is reloaded with one greater than it would have been.
    // https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware
    if (frame_seq_step_counter == 7) {
        envelope.timer++;
    }
    lfsr = 0x7FFF;
}

template<bool has_sweep> f32 PulseChannel<has_sweep>::GetOutput()
{
    static constexpr std::array
      duty_table = { 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0 };
    return enabled * dac_enabled * volume * duty_table[8 * duty + wave_pos] / 7.5f - 1.0f;
}

template<bool has_sweep> void PulseChannel<has_sweep>::Initialize()
{
    dac_enabled = enabled = false;
    volume = duty = wave_pos = 0;
    output = 0.0f;
    envelope.Initialize();
    length_counter.Initialize();
    sweep.Initialize();
}

template<bool has_sweep> void PulseChannel<has_sweep>::Step()
{
    if (timer == 0) {
        timer = (2048 - freq) * 4;
        wave_pos = (wave_pos + 1) & 7;
    } else {
        --timer;
    }
}

template<bool has_sweep> void PulseChannel<has_sweep>::Trigger()
{
    if (dac_enabled) {
        Enable();
    }
    if constexpr (has_sweep) {
        sweep.Enable();
    }
    envelope.Enable();
    // Enabling in first half of length period should clock length
    // dmg_sound 03-trigger test rom
    // TODO
    if (length_counter.value == 0) {
        length_counter.value = 64;
        if (length_counter.enabled && frame_seq_step_counter <= 3) {
            --length_counter.value;
        }
    }
    // When triggering a square channel, the low two bits of the frequency timer are NOT modified.
    // https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware#Obscure_Behavior
    timer = timer & 3 | freq & ~3;
    envelope.timer = envelope.period > 0 ? envelope.period : 8;
    // If a channel is triggered when the frame sequencer's next step will clock the volume envelope,
    // the envelope's timer is reloaded with one greater than it would have been.
    // https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware#Obscure_Behavior
    if (frame_seq_step_counter == 7) {
        envelope.timer++;
    }
}

void Sweep::Clock()
{
    if (timer > 0) {
        timer--;
    }
    if (timer == 0) {
        timer = period > 0 ? period : 8;
        if (enabled && period > 0) {
            auto new_freq = ComputeNewFreq();
            if (new_freq < 2048 && shift > 0) {
                // update shadow frequency and CH1 frequency registers with new frequency
                shadow_freq = new_freq;
                nr13 = new_freq & 0xFF;
                nr14 = (new_freq >> 8) & 7;
                ComputeNewFreq();
            }
        }
    }
}

uint Sweep::ComputeNewFreq()
{
    uint new_freq = shadow_freq >> shift;
    if (direction == Direction::Increasing) {
        new_freq = shadow_freq + new_freq;
    } else {
        new_freq = shadow_freq - new_freq;
    }
    if (new_freq >= 2048) {
        ch->Disable();
    }
    if (direction == Direction::Decreasing) {
        negate_has_been_used = true;
    }
    return new_freq;
}

void Sweep::Enable()
{
    shadow_freq = ch->freq;
    timer = period > 0 ? period : 8;
    enabled = period != 0 || shift != 0;
    negate_has_been_used = false;
    if (shift > 0) {
        ComputeNewFreq();
    }
}

void Sweep::Initialize()
{
    enabled = negate_has_been_used = false;
    period = shadow_freq = shift = timer = 0;
    direction = Direction::Decreasing;
}

f32 WaveChannel::GetOutput()
{
    if (enabled && dac_enabled) {
        auto sample = sample_buffer;
        if (wave_pos & 1) {
            sample &= 0xF;
        } else {
            sample >>= 4;
        }
        static constexpr std::array output_level_shift = { 4, 0, 1, 2 };
        sample >>= output_level_shift[output_level];
        return sample / 7.5f - 1.0f;
    } else {
        return 0.0f;
    }
}

void WaveChannel::Initialize()
{
    dac_enabled = enabled = false;
    volume = wave_pos = output_level = sample_buffer = 0;
    output = 0.0f;
    length_counter.Initialize();
}

void WaveChannel::Step()
{
    if (timer == 0) {
        timer = (2048 - freq) * 2;
        wave_pos = (wave_pos + 1) & 0x1F;
        sample_buffer = wave_ram[wave_pos / 2];
        // wave_ram_accessible_by_cpu_when_ch3_enabled = true;
        // t_cycles_since_ch3_read_wave_ram = 0;
    } else {
        --timer;
    }
}

void WaveChannel::Trigger()
{
    if (dac_enabled) {
        Enable();
    }
    if (length_counter.value == 0) {
        length_counter.value = 256;
        if (length_counter.enabled && frame_seq_step_counter <= 3) {
            --length_counter.value;
        }
    }
    // Reload period.
    // The low two bits of the frequency timer are NOT modified.
    // https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware#Obscure_Behavior
    timer = timer & 3 | freq & ~3;
    wave_pos = 0;
}

template u8 ReadReg<u8>(u32);
template s8 ReadReg<s8>(u32);
template u16 ReadReg<u16>(u32);
template s16 ReadReg<s16>(u32);
template u32 ReadReg<u32>(u32);
template s32 ReadReg<s32>(u32);
template void WriteReg<u8>(u32, u8);
template void WriteReg<s8>(u32, s8);
template void WriteReg<u16>(u32, u16);
template void WriteReg<s16>(u32, s16);
template void WriteReg<u32>(u32, u32);
template void WriteReg<s32>(u32, s32);

} // namespace gba::apu
