#include <windows.h>

#include <audioenginebaseapo.h>
#include <ks.h>
#include <ksmedia.h>
#include <mmreg.h>
#include <wrl/implements.h>
#include <wrl/module.h>

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <span>

#include "canary/biquad.h"

namespace canary {

namespace {

using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;

constexpr double kLowCutFrequency = 400.0;
constexpr double kHighCutFrequency = 2500.0;
constexpr double kCutDecibels = -30.0;

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

class __declspec(uuid("B22EF1FC-57E7-4E77-AC91-1D232BA20CF2")) CanaryApo final
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IAudioProcessingObject, IAudioProcessingObjectRT,
                          IAudioProcessingObjectConfiguration, IAudioSystemEffects> {
public:
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
        result->clsid = __uuidof(CanaryApo);
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

    STDMETHODIMP Initialize(UINT32, BYTE*) override
    {
        return S_OK;
    }

    STDMETHODIMP IsInputFormatSupported(IAudioMediaType* outputFormat, IAudioMediaType* requestedInputFormat,
                                        IAudioMediaType** supportedInputFormat) override
    {
        return negotiate(outputFormat, requestedInputFormat, supportedInputFormat);
    }

    STDMETHODIMP IsOutputFormatSupported(IAudioMediaType* inputFormat, IAudioMediaType* requestedOutputFormat,
                                         IAudioMediaType** supportedOutputFormat) override
    {
        return negotiate(inputFormat, requestedOutputFormat, supportedOutputFormat);
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
            return E_INVALIDARG;
        }
        UNCOMPRESSEDAUDIOFORMAT format{};
        if (!readFloatFormat(inputs[0]->pFormat, format)) {
            return APOERR_FORMAT_NOT_SUPPORTED;
        }

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

        if (input->u32BufferFlags == BUFFER_VALID && channels_ > 0) {
            std::span<float> samples(reinterpret_cast<float*>(input->pBuffer), static_cast<std::size_t>(frames) * channels_);
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
    UINT32 channels_ = 0;
    bool locked_ = false;
    Biquad lowCut_;
    Biquad highCut_;
};

CoCreatableClass(CanaryApo);

}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void** object)
{
    return Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule().GetClassObject(clsid, iid, object);
}

STDAPI DllCanUnloadNow()
{
    return Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule().Terminate() ? S_OK : S_FALSE;
}
