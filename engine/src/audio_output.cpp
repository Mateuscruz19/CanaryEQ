#include "canary/audio_output.h"

#include <windows.h>

#include <audioclient.h>
#include <avrt.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include <xmmintrin.h>
#include <pmmintrin.h>

#include <cstdio>

namespace canary {

namespace {

using Microsoft::WRL::ComPtr;

constexpr REFERENCE_TIME kBufferDuration = 200'000;
constexpr DWORD kWaitTimeoutMs = 2000;

std::string describe(const char* step, HRESULT hr)
{
    char text[128];
    std::snprintf(text, sizeof(text), "%s failed (0x%08lX)", step, static_cast<unsigned long>(hr));
    return text;
}

struct ComScope {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    ~ComScope()
    {
        if (SUCCEEDED(hr)) {
            CoUninitialize();
        }
    }
};

struct EventHandle {
    HANDLE handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);

    ~EventHandle()
    {
        if (handle) {
            CloseHandle(handle);
        }
    }
};

struct ProAudioScope {
    DWORD taskIndex = 0;
    HANDLE handle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    ~ProAudioScope()
    {
        if (handle) {
            AvRevertMmThreadCharacteristics(handle);
        }
    }
};

WAVEFORMATEXTENSIBLE floatFormat(std::uint32_t sampleRate, std::uint32_t channels)
{
    WAVEFORMATEXTENSIBLE format{};
    format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    format.Format.nChannels = static_cast<WORD>(channels);
    format.Format.nSamplesPerSec = sampleRate;
    format.Format.wBitsPerSample = 32;
    format.Format.nBlockAlign = static_cast<WORD>(channels * sizeof(float));
    format.Format.nAvgBytesPerSec = sampleRate * format.Format.nBlockAlign;
    format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    format.Samples.wValidBitsPerSample = 32;
    format.dwChannelMask = channels == 1 ? KSAUDIO_SPEAKER_MONO
                         : channels == 2 ? KSAUDIO_SPEAKER_STEREO
                                         : 0;
    format.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    return format;
}

}

AudioOutput::~AudioOutput()
{
    stop();
}

bool AudioOutput::start(std::uint32_t sampleRate, std::uint32_t channels, AudioRenderer& renderer)
{
    stop();

    std::promise<std::string> ready;
    std::future<std::string> result = ready.get_future();

    thread_ = std::jthread([sampleRate, channels, &renderer, &ready](std::stop_token stop) {
        run(stop, sampleRate, channels, renderer, ready);
    });

    error_ = result.get();
    if (!error_.empty()) {
        stop();
        return false;
    }
    return true;
}

void AudioOutput::stop()
{
    if (thread_.joinable()) {
        thread_.request_stop();
        thread_.join();
    }
}

const std::string& AudioOutput::error() const
{
    return error_;
}

void AudioOutput::run(std::stop_token stop, std::uint32_t sampleRate, std::uint32_t channels,
                      AudioRenderer& renderer, std::promise<std::string>& ready)
{
    auto failed = [&ready](HRESULT hr, const char* step) {
        if (FAILED(hr)) {
            ready.set_value(describe(step, hr));
            return true;
        }
        return false;
    };

    ComScope com;
    if (failed(com.hr, "CoInitializeEx")) {
        return;
    }

    ComPtr<IMMDeviceEnumerator> enumerator;
    if (failed(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)),
               "CoCreateInstance(MMDeviceEnumerator)")) {
        return;
    }

    ComPtr<IMMDevice> device;
    if (failed(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device), "GetDefaultAudioEndpoint")) {
        return;
    }

    ComPtr<IAudioClient> client;
    if (failed(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                reinterpret_cast<void**>(client.GetAddressOf())),
               "IMMDevice::Activate")) {
        return;
    }

    WAVEFORMATEXTENSIBLE format = floatFormat(sampleRate, channels);
    DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
                | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    if (failed(client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, kBufferDuration, 0, &format.Format, nullptr),
               "IAudioClient::Initialize")) {
        return;
    }

    EventHandle event;
    if (!event.handle) {
        ready.set_value("CreateEvent failed");
        return;
    }
    if (failed(client->SetEventHandle(event.handle), "IAudioClient::SetEventHandle")) {
        return;
    }

    UINT32 bufferFrames = 0;
    if (failed(client->GetBufferSize(&bufferFrames), "IAudioClient::GetBufferSize")) {
        return;
    }

    ComPtr<IAudioRenderClient> renderClient;
    if (failed(client->GetService(IID_PPV_ARGS(&renderClient)), "IAudioClient::GetService")) {
        return;
    }

    auto fill = [&](UINT32 frames) {
        BYTE* data = nullptr;
        if (FAILED(renderClient->GetBuffer(frames, &data))) {
            return false;
        }
        std::span<float> buffer(reinterpret_cast<float*>(data), static_cast<std::size_t>(frames) * channels);
        renderer.render(buffer, channels);
        return SUCCEEDED(renderClient->ReleaseBuffer(frames, 0));
    };

    if (!fill(bufferFrames)) {
        ready.set_value("initial buffer fill failed");
        return;
    }
    if (failed(client->Start(), "IAudioClient::Start")) {
        return;
    }

    ready.set_value({});

    ProAudioScope proAudio;
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
    std::stop_callback wake(stop, [&event] { SetEvent(event.handle); });

    while (!stop.stop_requested()) {
        if (WaitForSingleObject(event.handle, kWaitTimeoutMs) != WAIT_OBJECT_0) {
            break;
        }
        if (stop.stop_requested()) {
            break;
        }

        UINT32 padding = 0;
        if (FAILED(client->GetCurrentPadding(&padding))) {
            break;
        }

        UINT32 available = bufferFrames - padding;
        if (available > 0 && !fill(available)) {
            break;
        }
    }

    client->Stop();
}

}
