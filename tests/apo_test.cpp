#include <windows.h>

#include <audioenginebaseapo.h>
#include <ks.h>
#include <ksmedia.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>

namespace {

using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;

constexpr CLSID kCanaryApoClsid = {0xB22EF1FC, 0x57E7, 0x4E77, {0xAC, 0x91, 0x1D, 0x23, 0x2B, 0xA2, 0x0C, 0xF2}};
constexpr float kSampleRate = 48000.0f;
constexpr UINT32 kChannels = 2;

class FloatMediaType : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IAudioMediaType> {
public:
    STDMETHODIMP IsCompressedFormat(BOOL* compressed) override
    {
        *compressed = FALSE;
        return S_OK;
    }

    STDMETHODIMP IsEqual(IAudioMediaType*, DWORD* flags) override
    {
        *flags = 0;
        return S_OK;
    }

    STDMETHODIMP_(const WAVEFORMATEX*) GetAudioFormat() override
    {
        return nullptr;
    }

    STDMETHODIMP GetUncompressedAudioFormat(UNCOMPRESSEDAUDIOFORMAT* format) override
    {
        *format = {};
        format->guidFormatType = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        format->dwSamplesPerFrame = kChannels;
        format->dwBytesPerSampleContainer = 4;
        format->dwValidBitsPerSample = 32;
        format->fFramesPerSecond = kSampleRate;
        return S_OK;
    }
};

int failures = 0;

void check(bool condition, const std::string& what)
{
    std::printf("%s %s\n", condition ? "ok  " : "FAIL", what.c_str());
    if (!condition) {
        ++failures;
    }
}

double levelAfterApo(IAudioProcessingObjectRT* rt, double frequency)
{
    constexpr UINT32 frames = 480;
    std::vector<float> buffer(frames * kChannels);
    APO_CONNECTION_PROPERTY connection{};
    APO_CONNECTION_PROPERTY* connections[] = {&connection};

    double loudest = 0.0;
    for (UINT32 block = 0; block < 200; ++block) {
        for (UINT32 frame = 0; frame < frames; ++frame) {
            double t = static_cast<double>(block * frames + frame) / kSampleRate;
            float value = static_cast<float>(0.5 * std::sin(2.0 * std::numbers::pi * frequency * t));
            buffer[frame * kChannels] = value;
            buffer[frame * kChannels + 1] = value;
        }
        connection.pBuffer = reinterpret_cast<UINT_PTR>(buffer.data());
        connection.u32ValidFrameCount = frames;
        connection.u32BufferFlags = BUFFER_VALID;
        rt->APOProcess(1, connections, 1, connections);
        if (block >= 100) {
            for (float sample : buffer) {
                loudest = std::max(loudest, static_cast<double>(std::fabs(sample)));
            }
        }
    }
    return 20.0 * std::log10(loudest / 0.5);
}

}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: canary-apo-test <canary_apo.dll>\n");
        return 1;
    }

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    HMODULE module = LoadLibraryA(argv[1]);
    check(module != nullptr, "the APO DLL loads");
    if (!module) {
        return 1;
    }

    using GetClassObject = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, void**);
    auto getClassObject = reinterpret_cast<GetClassObject>(GetProcAddress(module, "DllGetClassObject"));
    check(getClassObject != nullptr, "DllGetClassObject is exported");
    check(GetProcAddress(module, "DllCanUnloadNow") != nullptr, "DllCanUnloadNow is exported");
    if (!getClassObject) {
        return 1;
    }

    ComPtr<IClassFactory> factory;
    check(SUCCEEDED(getClassObject(kCanaryApoClsid, IID_PPV_ARGS(&factory))), "the class factory is found by CLSID");
    if (!factory) {
        return 1;
    }

    ComPtr<IAudioProcessingObject> apo;
    check(SUCCEEDED(factory->CreateInstance(nullptr, IID_PPV_ARGS(&apo))), "the APO is created");
    if (!apo) {
        return 1;
    }

    ComPtr<IAudioProcessingObjectRT> rt;
    ComPtr<IAudioProcessingObjectConfiguration> configuration;
    ComPtr<IAudioSystemEffects> systemEffects;
    check(SUCCEEDED(apo.As(&rt)), "it implements IAudioProcessingObjectRT");
    check(SUCCEEDED(apo.As(&configuration)), "it implements IAudioProcessingObjectConfiguration");
    check(SUCCEEDED(apo.As(&systemEffects)), "it is marked as a system effect");

    APO_REG_PROPERTIES* properties = nullptr;
    check(SUCCEEDED(apo->GetRegistrationProperties(&properties)) && properties, "registration properties are returned");
    if (properties) {
        check(IsEqualCLSID(properties->clsid, kCanaryApoClsid), "registration reports the right CLSID");
        check(properties->Flags == 13, "registration flags match the registry entry (13)");
        check(IsEqualIID(properties->iidAPOInterfaceList[0], __uuidof(IAudioProcessingObject)),
              "registration lists IAudioProcessingObject");
        CoTaskMemFree(properties);
    }

    check(SUCCEEDED(apo->Initialize(0, nullptr)), "Initialize succeeds");

    ComPtr<IAudioMediaType> format = Make<FloatMediaType>();
    ComPtr<IAudioMediaType> supported;
    check(apo->IsInputFormatSupported(nullptr, format.Get(), &supported) == S_OK && supported,
          "32-bit float input is accepted");

    APO_CONNECTION_DESCRIPTOR descriptor{};
    descriptor.pFormat = format.Get();
    APO_CONNECTION_DESCRIPTOR* descriptors[] = {&descriptor};
    check(SUCCEEDED(configuration->LockForProcess(1, descriptors, 1, descriptors)), "LockForProcess accepts the format");

    UINT32 channels = 0;
    check(SUCCEEDED(apo->GetInputChannelCount(&channels)) && channels == kChannels, "it reports the locked channel count");

    double low = levelAfterApo(rt.Get(), 80.0);
    double mid = levelAfterApo(rt.Get(), 1000.0);
    double high = levelAfterApo(rt.Get(), 10000.0);
    std::printf("     80 Hz %+.1f dB, 1 kHz %+.1f dB, 10 kHz %+.1f dB\n", low, mid, high);
    check(low < -20.0, "the telephone effect cuts low frequencies");
    check(mid > -10.0, "the telephone effect keeps the voice range");
    check(high < -20.0, "the telephone effect cuts high frequencies");

    check(SUCCEEDED(configuration->UnlockForProcess()), "UnlockForProcess succeeds");

    std::printf("%s\n", failures == 0 ? "all APO checks passed" : "APO checks failed");
    return failures == 0 ? 0 : 1;
}
