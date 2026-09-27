#include <dr_wav.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

#include "canary/audio_output.h"
#include "looping_player.h"

int main(int argc, char** argv)
{
    if (argc < 2) {
        spdlog::error("usage: canary <file.wav> [gain_db]");
        return 1;
    }

    const char* wavPath = argv[1];
    float gainDecibels = argc >= 3 ? std::strtof(argv[2], nullptr) : 0.0f;

    unsigned int channels = 0;
    unsigned int sampleRate = 0;
    drwav_uint64 frames = 0;
    float* data = drwav_open_file_and_read_pcm_frames_f32(wavPath, &channels, &sampleRate, &frames, nullptr);
    if (!data || frames == 0) {
        spdlog::error("could not read {}", wavPath);
        drwav_free(data, nullptr);
        return 1;
    }

    std::vector<float> samples(data, data + frames * channels);
    drwav_free(data, nullptr);

    canary::LoopingPlayer player(std::move(samples));
    player.setGainDecibels(gainDecibels);

    canary::AudioOutput output;
    if (!output.start(sampleRate, channels, player)) {
        spdlog::error("audio error: {}", output.error());
        return 1;
    }

    spdlog::info("playing {} ({} Hz, {} ch) at {:+.1f} dB, press Enter to stop",
                 wavPath, sampleRate, channels, gainDecibels);
    std::getchar();

    output.stop();
    return 0;
}
