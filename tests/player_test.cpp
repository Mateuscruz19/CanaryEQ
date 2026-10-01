#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "looping_player.h"

namespace {

constexpr double kSampleRate = 48000.0;
constexpr std::size_t kChannels = 2;
constexpr std::size_t kBlockFrames = 480;

canary::LoopingPlayer constantPlayer(float value)
{
    return canary::LoopingPlayer(std::vector<float>(kBlockFrames * kChannels * 4, value), kSampleRate, 0.0f);
}

std::vector<float> renderBlocks(canary::LoopingPlayer& player, int blocks)
{
    std::vector<float> buffer(kBlockFrames * kChannels);
    for (int i = 0; i < blocks; ++i) {
        player.render(buffer, kChannels);
    }
    return buffer;
}

}

TEST_CASE("the player loops the song back to the start")
{
    canary::LoopingPlayer player({0.1f, 0.2f, 0.3f, 0.4f}, kSampleRate, 0.0f);
    std::vector<float> buffer(6);
    player.render(buffer, kChannels);

    CHECK(buffer == std::vector<float>{0.1f, 0.2f, 0.3f, 0.4f, 0.1f, 0.2f});
}

TEST_CASE("moving the volume changes the audio that comes out")
{
    canary::LoopingPlayer player = constantPlayer(0.5f);
    player.setGainDecibels(-6.0206f);
    std::vector<float> out = renderBlocks(player, 2);

    CHECK(out.front() == doctest::Approx(0.25f).epsilon(0.001));
    CHECK(out.back() == doctest::Approx(0.25f).epsilon(0.001));
}

TEST_CASE("moving the balance right lowers only the left side")
{
    canary::LoopingPlayer player = constantPlayer(0.5f);
    player.setBalanceDecibels(6.0206f);
    std::vector<float> out = renderBlocks(player, 2);

    CHECK(out[0] == doctest::Approx(0.25f).epsilon(0.001));
    CHECK(out[1] == doctest::Approx(0.5f));
}

TEST_CASE("boosting an EQ band lowers the preamp so the rest of the song gets quieter")
{
    canary::LoopingPlayer player = constantPlayer(0.5f);
    player.setEqDecibels(canary::Band::Treble, 6.0206f);
    std::vector<float> out = renderBlocks(player, 40);

    CHECK(out[0] == doctest::Approx(0.25f).epsilon(0.01));
    CHECK(out[1] == doctest::Approx(0.25f).epsilon(0.01));
}

TEST_CASE("cutting an EQ band never makes the untouched part quieter")
{
    canary::LoopingPlayer player = constantPlayer(0.5f);
    player.setEqDecibels(canary::Band::Treble, -12.0f);
    std::vector<float> out = renderBlocks(player, 40);

    CHECK(out[0] == doctest::Approx(0.5f).epsilon(0.01));
}

TEST_CASE("boosting the bass makes a low tone louder than a high tone")
{
    auto toneLevel = [](double frequency) {
        std::vector<float> song(48000 * kChannels);
        for (std::size_t frame = 0; frame < 48000; ++frame) {
            float value = static_cast<float>(0.25 * std::sin(2.0 * 3.141592653589793 * frequency * frame / kSampleRate));
            song[frame * kChannels] = value;
            song[frame * kChannels + 1] = value;
        }
        canary::LoopingPlayer player(std::move(song), kSampleRate, 0.0f);
        player.setEqDecibels(canary::Band::Bass, 12.0f);
        renderBlocks(player, 60);
        std::vector<float> out = renderBlocks(player, 10);
        float loudest = 0.0f;
        for (float sample : out) {
            loudest = std::max(loudest, std::fabs(sample));
        }
        return loudest;
    };

    float low = toneLevel(40.0);
    float high = toneLevel(5000.0);
    CHECK(low > high * 3.0f);
    CHECK(low <= 0.26f);
}
