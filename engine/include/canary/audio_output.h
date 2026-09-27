#pragma once

#include <cstdint>
#include <future>
#include <span>
#include <string>
#include <thread>

namespace canary {

class AudioRenderer {
public:
    virtual ~AudioRenderer() = default;
    virtual void render(std::span<float> buffer, std::uint32_t channels) = 0;
};

class AudioOutput {
public:
    AudioOutput() = default;
    ~AudioOutput();

    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    bool start(std::uint32_t sampleRate, std::uint32_t channels, AudioRenderer& renderer);
    void stop();
    const std::string& error() const;

private:
    static void run(std::stop_token stop, std::uint32_t sampleRate, std::uint32_t channels,
                    AudioRenderer& renderer, std::promise<std::string>& ready);

    std::jthread thread_;
    std::string error_;
};

}
