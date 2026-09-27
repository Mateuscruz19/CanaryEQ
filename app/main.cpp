#include <dr_wav.h>

#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

#include "canary/audio_output.h"
#include "looping_player.h"

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: canary <file.wav> [gain_db]\n");
        return 1;
    }

    const char* path = argv[1];
    float gainDecibels = argc >= 3 ? std::strtof(argv[2], nullptr) : 0.0f;

    unsigned int channels = 0;
    unsigned int sampleRate = 0;
    drwav_uint64 frames = 0;
    float* data = drwav_open_file_and_read_pcm_frames_f32(path, &channels, &sampleRate, &frames, nullptr);
    if (!data || frames == 0) {
        std::fprintf(stderr, "could not read %s\n", path);
        drwav_free(data, nullptr);
        return 1;
    }

    std::vector<float> samples(data, data + frames * channels);
    drwav_free(data, nullptr);

    canary::LoopingPlayer player(std::move(samples));
    player.setGainDecibels(gainDecibels);

    canary::AudioOutput output;
    if (!output.start(sampleRate, channels, player)) {
        std::fprintf(stderr, "audio error: %s\n", output.error().c_str());
        return 1;
    }

    std::printf("playing %s (%u Hz, %u ch) at %+.1f dB, press Enter to stop\n",
                path, sampleRate, channels, gainDecibels);
    std::getchar();

    output.stop();
    return 0;
}
