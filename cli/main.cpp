#include <dr_wav.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>
#include <span>
#include <string_view>
#include <vector>

#include "canary/balance.h"
#include "canary/gain.h"
#include "canary/three_band_eq.h"

namespace {

constexpr std::size_t kBlockFrames = 480;

struct Settings {
    const char* inputPath = nullptr;
    const char* outputPath = nullptr;
    float gain = 0.0f;
    float balance = 0.0f;
    float bass = 0.0f;
    float mid = 0.0f;
    float treble = 0.0f;
};

bool parse(int argc, char** argv, Settings& settings)
{
    if (argc < 3) {
        return false;
    }
    settings.inputPath = argv[1];
    settings.outputPath = argv[2];

    for (int i = 3; i + 1 < argc; i += 2) {
        std::string_view option = argv[i];
        char* end = nullptr;
        float value = std::strtof(argv[i + 1], &end);
        if (end == argv[i + 1] || *end != '\0') {
            spdlog::error("invalid value for {}: {}", option, argv[i + 1]);
            return false;
        }

        if (option == "--gain") {
            settings.gain = value;
        }
        else if (option == "--balance") {
            settings.balance = value;
        }
        else if (option == "--bass") {
            settings.bass = value;
        }
        else if (option == "--mid") {
            settings.mid = value;
        }
        else if (option == "--treble") {
            settings.treble = value;
        }
        else {
            spdlog::error("unknown option {}", option);
            return false;
        }
    }
    return (argc - 3) % 2 == 0;
}

}

int main(int argc, char** argv)
{
    Settings settings;
    if (!parse(argc, argv, settings)) {
        spdlog::error("usage: canary-cli <in.wav> <out.wav> [--gain dB] [--balance dB] [--bass dB] [--mid dB] [--treble dB]");
        return 1;
    }

    unsigned int channels = 0;
    unsigned int sampleRate = 0;
    drwav_uint64 frames = 0;
    float* data = drwav_open_file_and_read_pcm_frames_f32(settings.inputPath, &channels, &sampleRate, &frames, nullptr);
    if (!data || frames == 0) {
        spdlog::error("could not read {}", settings.inputPath);
        drwav_free(data, nullptr);
        return 1;
    }
    std::vector<float> samples(data, data + frames * channels);
    drwav_free(data, nullptr);

    canary::ThreeBandEq eq(sampleRate);
    eq.setGainDecibels(canary::Band::Bass, settings.bass);
    eq.setGainDecibels(canary::Band::Mid, settings.mid);
    eq.setGainDecibels(canary::Band::Treble, settings.treble);
    eq.snapToTarget();

    canary::Balance balance;
    balance.setDecibels(settings.balance);
    balance.snapToTarget();

    canary::Gain gain;
    gain.setDecibels(settings.gain);
    gain.snapToTarget();

    std::span<float> all(samples);
    for (std::size_t offset = 0; offset < all.size(); offset += kBlockFrames * channels) {
        std::span<float> block = all.subspan(offset, std::min(kBlockFrames * channels, all.size() - offset));
        eq.process(block, channels);
        balance.process(block, channels);
        gain.process(block, channels);
    }

    drwav_data_format format{};
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = channels;
    format.sampleRate = sampleRate;
    format.bitsPerSample = 32;

    drwav wav;
    if (!drwav_init_file_write(&wav, settings.outputPath, &format, nullptr)) {
        spdlog::error("could not write {}", settings.outputPath);
        return 1;
    }
    drwav_write_pcm_frames(&wav, frames, samples.data());
    drwav_uninit(&wav);

    spdlog::info("wrote {} (preamp {:+.1f} dB)", settings.outputPath, eq.preampDecibels());
    return 0;
}
