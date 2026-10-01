#include <windows.h>

#include <audioenginebaseapo.h>
#include <ks.h>
#include <ksmedia.h>
#include <mmreg.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <initializer_list>
#include <new>
#include <span>

#include "canary/biquad.h"

namespace canary {

namespace {

constexpr CLSID kCanaryApoClsid = {0xB22EF1FC, 0x57E7, 0x4E77, {0xAC, 0x91, 0x1D, 0x23, 0x2B, 0xA2, 0x0C, 0xF2}};
constexpr double kLowCutFrequency = 400.0;
constexpr double kHighCutFrequency = 2500.0;
constexpr double kCutDecibels = -30.0;

std::atomic<long> liveObjects{0};
std::atomic<long> serverLocks{0};

void logLine(const void* instance, const char* format, ...)
{
    char message[512];
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

    SYSTEMTIME now;
    GetLocalTime(&now);
    char line[640];
    int length = std::snprintf(line, sizeof(line), "%02d:%02d:%02d.%03d pid=%lu apo=%p %s\r\n", now.wHour, now.wMinute,
                               now.wSecond, now.wMilliseconds, GetCurrentProcessId(), instance, message);

    for (const wchar_t* location : {L"%ProgramData%\\CanaryEQ\\apo-log.txt", L"%TEMP%\\canary-apo-log.txt"}) {
        wchar_t path[MAX_PATH];
        if (ExpandEnvironmentStringsW(location, path, MAX_PATH) == 0) {
            continue;
        }
        HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            continue;
        }
        DWORD written = 0;
        WriteFile(file, line, static_cast<DWORD>(std::max(length, 0)), &written, nullptr);
        CloseHandle(file);
    }
}

const char* describe(IAudioMediaType* type, char* buffer, std::size_t size)
{
    UNCOMPRESSEDAUDIOFORMAT format{};
    if (!type) {
        std::snprintf(buffer, size, "none");
    }
    else if (FAILED(type->GetUncompressedAudioFormat(&format))) {
        std::snprintf(buffer, size, "compressed/unknown");
    }
    else {
        const char* kind = format.guidFormatType == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT ? "float"
                         : format.guidFormatType == KSDATAFORMAT_SUBTYPE_PCM        ? "pcm"
                                                                                     : "other";
        std::snprintf(buffer, size, "%s %luch %.0fHz %lubytes", kind, format.dwSamplesPerFrame,
                      format.fFramesPerSecond, format.dwBytesPerSampleContainer);
    }
    return buffer;
}

bool readFloatFormat(IAudioMediaType* type, UNCOMPRESSEDAUDIOFORMAT& format)
{
    if (!type || FAILED(type->GetUncompressedAudioFormat(&format))) {
        return false;
    }
    return format.guidFormatType == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT && format.dwBytesPerSampleContainer == 4;
}

bool sameFormat(const UNCOMPRESSEDAUDIOFORMAT& a, const UNCOMPRESSEDAUDIOFORMAT& b)
{
    return a.dwSamplesPerFrame == b.dwSamplesPerFrame && a.fFramesPerSecond == b.fFramesPerSecond &&
           a.dwBytesPerSampleContainer == b.dwBytesPerSampleContainer && a.guidFormatType == b.guidFormatType;
}

HRESULT negotiate(IAudioMediaType* opposite, IAudioMediaType* requested, IAudioMediaType** supported)
{
    if (!requested || !supported) {
        return E_POINTER;
    }
    *supported = nullptr;

    UNCOMPRESSEDAUDIOFORMAT requestedFormat{};
    if (!readFloatFormat(requested, requestedFormat)) {
        return APOERR_FORMAT_NOT_SUPPORTED;
    }

    UNCOMPRESSEDAUDIOFORMAT oppositeFormat{};
    if (opposite && readFloatFormat(opposite, oppositeFormat) && !sameFormat(oppositeFormat, requestedFormat)) {
        opposite->AddRef();
        *supported = opposite;
        return S_FALSE;
    }

    requested->AddRef();
    *supported = requested;
    return S_OK;
}

}

class CanaryApo final : public IAudioProcessingObject,
                        public IAudioProcessingObjectRT,
                        public IAudioProcessingObjectConfiguration,
                        public IAudioSystemEffects {
public:
    explicit CanaryApo(IUnknown* outer)
        : inner_(this)
        , outer_(outer ? outer : &inner_)
    {
        liveObjects.fetch_add(1);
        logLine(this, "created aggregated=%d", outer != nullptr);
    }

    ~CanaryApo()
    {
        logLine(this, "destroyed");
        liveObjects.fetch_sub(1);
    }

    CanaryApo(const CanaryApo&) = delete;
    CanaryApo& operator=(const CanaryApo&) = delete;

    IUnknown* inner()
    {
        return &inner_;
    }

    STDMETHODIMP QueryInterface(REFIID iid, void** object) override
    {
        return outer_->QueryInterface(iid, object);
    }

    STDMETHODIMP_(ULONG) AddRef() override
    {
        return outer_->AddRef();
    }

    STDMETHODIMP_(ULONG) Release() override
    {
        return outer_->Release();
    }

    STDMETHODIMP Reset() override
    {
        lowCut_.reset();
        highCut_.reset();
        return S_OK;
    }

    STDMETHODIMP GetLatency(HNSTIME* time) override
    {
        if (!time) {
            return E_POINTER;
        }
        *time = 0;
        return S_OK;
    }

    STDMETHODIMP GetRegistrationProperties(APO_REG_PROPERTIES** properties) override
    {
        if (!properties) {
            return E_POINTER;
        }
        auto* result = static_cast<APO_REG_PROPERTIES*>(CoTaskMemAlloc(sizeof(APO_REG_PROPERTIES)));
        if (!result) {
            return E_OUTOFMEMORY;
        }
        *result = {};
        result->clsid = kCanaryApoClsid;
        result->Flags = static_cast<APO_FLAG>(APO_FLAG_INPLACE | APO_FLAG_FRAMESPERSECOND_MUST_MATCH |
                                              APO_FLAG_BITSPERSAMPLE_MUST_MATCH);
        wcscpy_s(result->szFriendlyName, L"CanaryEQ");
        wcscpy_s(result->szCopyrightInfo, L"Copyright (c) 2026 Mateus Cruz");
        result->u32MajorVersion = 0;
        result->u32MinorVersion = 1;
        result->u32MinInputConnections = 1;
        result->u32MaxInputConnections = 1;
        result->u32MinOutputConnections = 1;
        result->u32MaxOutputConnections = 1;
        result->u32MaxInstances = UINT32_MAX;
        result->u32NumAPOInterfaces = 1;
        result->iidAPOInterfaceList[0] = __uuidof(IAudioProcessingObject);
        *properties = result;
        return S_OK;
    }

    STDMETHODIMP Initialize(UINT32 size, BYTE*) override
    {
        logLine(this, "Initialize size=%u", size);
        return S_OK;
    }

    STDMETHODIMP IsInputFormatSupported(IAudioMediaType* outputFormat, IAudioMediaType* requestedInputFormat,
                                        IAudioMediaType** supportedInputFormat) override
    {
        HRESULT result = negotiate(outputFormat, requestedInputFormat, supportedInputFormat);
        char opposite[96];
        char requested[96];
        logLine(this, "IsInputFormatSupported opposite=[%s] requested=[%s] -> 0x%08lX",
                describe(outputFormat, opposite, sizeof(opposite)),
                describe(requestedInputFormat, requested, sizeof(requested)), static_cast<unsigned long>(result));
        return result;
    }

    STDMETHODIMP IsOutputFormatSupported(IAudioMediaType* inputFormat, IAudioMediaType* requestedOutputFormat,
                                         IAudioMediaType** supportedOutputFormat) override
    {
        HRESULT result = negotiate(inputFormat, requestedOutputFormat, supportedOutputFormat);
        char opposite[96];
        char requested[96];
        logLine(this, "IsOutputFormatSupported opposite=[%s] requested=[%s] -> 0x%08lX",
                describe(inputFormat, opposite, sizeof(opposite)),
                describe(requestedOutputFormat, requested, sizeof(requested)), static_cast<unsigned long>(result));
        return result;
    }

    STDMETHODIMP GetInputChannelCount(UINT32* count) override
    {
        if (!count) {
            return E_POINTER;
        }
        *count = channels_;
        return S_OK;
    }

    STDMETHODIMP LockForProcess(UINT32 inputCount, APO_CONNECTION_DESCRIPTOR** inputs, UINT32 outputCount,
                                APO_CONNECTION_DESCRIPTOR** outputs) override
    {
        if (inputCount < 1 || outputCount < 1 || !inputs || !outputs || !inputs[0]) {
            logLine(this, "LockForProcess invalid connections in=%u out=%u", inputCount, outputCount);
            return E_INVALIDARG;
        }
        char inputText[96];
        char outputText[96];
        logLine(this, "LockForProcess in=[%s] out=[%s] maxFrames=%u sameBuffer=%d",
                describe(inputs[0]->pFormat, inputText, sizeof(inputText)),
                describe(outputs[0] ? outputs[0]->pFormat : nullptr, outputText, sizeof(outputText)),
                inputs[0]->u32MaxFrameCount, outputs[0] && outputs[0]->pBuffer == inputs[0]->pBuffer);
        UNCOMPRESSEDAUDIOFORMAT format{};
        if (!readFloatFormat(inputs[0]->pFormat, format)) {
            logLine(this, "LockForProcess rejected: not 32-bit float");
            return APOERR_FORMAT_NOT_SUPPORTED;
        }
        processCalls_ = 0;
        validBuffers_ = 0;
        silentBuffers_ = 0;
        processedFrames_ = 0;

        channels_ = format.dwSamplesPerFrame;
        double sampleRate = format.fFramesPerSecond;
        lowCut_.setCoefficients(lowShelf(sampleRate, kLowCutFrequency, kCutDecibels));
        highCut_.setCoefficients(highShelf(sampleRate, kHighCutFrequency, kCutDecibels));
        Reset();
        locked_ = true;
        return S_OK;
    }

    STDMETHODIMP UnlockForProcess() override
    {
        locked_ = false;
        logLine(this, "UnlockForProcess calls=%llu valid=%llu silent=%llu frames=%llu", processCalls_.load(),
                validBuffers_.load(), silentBuffers_.load(), processedFrames_.load());
        return S_OK;
    }

    STDMETHODIMP_(void) APOProcess(UINT32 inputCount, APO_CONNECTION_PROPERTY** inputs, UINT32 outputCount,
                                   APO_CONNECTION_PROPERTY** outputs) override
    {
        if (!locked_ || inputCount < 1 || outputCount < 1 || !inputs || !outputs || !inputs[0] || !outputs[0]) {
            return;
        }
        APO_CONNECTION_PROPERTY* input = inputs[0];
        APO_CONNECTION_PROPERTY* output = outputs[0];
        UINT32 frames = input->u32ValidFrameCount;
        processCalls_.fetch_add(1, std::memory_order_relaxed);
        if (input->u32BufferFlags == BUFFER_VALID) {
            validBuffers_.fetch_add(1, std::memory_order_relaxed);
            processedFrames_.fetch_add(frames, std::memory_order_relaxed);
        }
        else if (input->u32BufferFlags == BUFFER_SILENT) {
            silentBuffers_.fetch_add(1, std::memory_order_relaxed);
        }

        if (input->u32BufferFlags == BUFFER_VALID && channels_ > 0) {
            std::span<float> samples(reinterpret_cast<float*>(input->pBuffer),
                                     static_cast<std::size_t>(frames) * channels_);
            lowCut_.process(samples, channels_);
            highCut_.process(samples, channels_);
            if (output->pBuffer != input->pBuffer) {
                std::copy(samples.begin(), samples.end(), reinterpret_cast<float*>(output->pBuffer));
            }
        }

        output->u32ValidFrameCount = frames;
        output->u32BufferFlags = input->u32BufferFlags;
    }

    STDMETHODIMP_(UINT32) CalcInputFrames(UINT32 outputFrameCount) override
    {
        return outputFrameCount;
    }

    STDMETHODIMP_(UINT32) CalcOutputFrames(UINT32 inputFrameCount) override
    {
        return inputFrameCount;
    }

private:
    class Inner final : public IUnknown {
    public:
        explicit Inner(CanaryApo* owner)
            : owner_(owner)
        {
        }

        STDMETHODIMP QueryInterface(REFIID iid, void** object) override
        {
            if (!object) {
                return E_POINTER;
            }
            if (iid == __uuidof(IUnknown)) {
                *object = static_cast<IUnknown*>(this);
            }
            else if (iid == __uuidof(IAudioProcessingObject)) {
                *object = static_cast<IAudioProcessingObject*>(owner_);
            }
            else if (iid == __uuidof(IAudioProcessingObjectRT)) {
                *object = static_cast<IAudioProcessingObjectRT*>(owner_);
            }
            else if (iid == __uuidof(IAudioProcessingObjectConfiguration)) {
                *object = static_cast<IAudioProcessingObjectConfiguration*>(owner_);
            }
            else if (iid == __uuidof(IAudioSystemEffects)) {
                *object = static_cast<IAudioSystemEffects*>(owner_);
            }
            else {
                *object = nullptr;
                return E_NOINTERFACE;
            }
            static_cast<IUnknown*>(*object)->AddRef();
            return S_OK;
        }

        STDMETHODIMP_(ULONG) AddRef() override
        {
            return refs_.fetch_add(1) + 1;
        }

        STDMETHODIMP_(ULONG) Release() override
        {
            ULONG remaining = refs_.fetch_sub(1) - 1;
            if (remaining == 0) {
                delete owner_;
            }
            return remaining;
        }

    private:
        CanaryApo* owner_;
        std::atomic<ULONG> refs_{1};
    };

    Inner inner_;
    IUnknown* outer_;
    UINT32 channels_ = 0;
    bool locked_ = false;
    Biquad lowCut_;
    Biquad highCut_;
    std::atomic<std::uint64_t> processCalls_{0};
    std::atomic<std::uint64_t> validBuffers_{0};
    std::atomic<std::uint64_t> silentBuffers_{0};
    std::atomic<std::uint64_t> processedFrames_{0};
};

namespace {

class ClassFactory final : public IClassFactory {
public:
    STDMETHODIMP QueryInterface(REFIID iid, void** object) override
    {
        if (!object) {
            return E_POINTER;
        }
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IClassFactory)) {
            *object = static_cast<IClassFactory*>(this);
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override
    {
        return 2;
    }

    STDMETHODIMP_(ULONG) Release() override
    {
        return 1;
    }

    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID iid, void** object) override
    {
        if (!object) {
            return E_POINTER;
        }
        *object = nullptr;
        if (outer && iid != __uuidof(IUnknown)) {
            logLine(nullptr, "CreateInstance refused: aggregation must ask for IUnknown");
            return CLASS_E_NOAGGREGATION;
        }

        auto* apo = new (std::nothrow) CanaryApo(outer);
        if (!apo) {
            return E_OUTOFMEMORY;
        }
        HRESULT result = apo->inner()->QueryInterface(iid, object);
        logLine(nullptr, "CreateInstance aggregated=%d -> 0x%08lX", outer != nullptr, static_cast<unsigned long>(result));
        apo->inner()->Release();
        return result;
    }

    STDMETHODIMP LockServer(BOOL lock) override
    {
        if (lock) {
            serverLocks.fetch_add(1);
        }
        else {
            serverLocks.fetch_sub(1);
        }
        return S_OK;
    }
};

ClassFactory classFactory;

}

}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        canary::logLine(nullptr, "dll loaded");
    }
    else if (reason == DLL_PROCESS_DETACH) {
        canary::logLine(nullptr, "dll unloaded");
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void** object)
{
    if (!object) {
        return E_POINTER;
    }
    *object = nullptr;
    if (clsid != canary::kCanaryApoClsid) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    HRESULT result = canary::classFactory.QueryInterface(iid, object);
    canary::logLine(nullptr, "DllGetClassObject -> 0x%08lX", static_cast<unsigned long>(result));
    return result;
}

STDAPI DllCanUnloadNow()
{
    return canary::liveObjects.load() == 0 && canary::serverLocks.load() == 0 ? S_OK : S_FALSE;
}
