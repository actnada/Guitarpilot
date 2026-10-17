#pragma once
// Host tests replace the device I/O boundary only. GuitarEngine.cpp is real.
#include <cstdint>
#include <memory>
namespace oboe {
enum class Result { OK };
enum class DataCallbackResult { Continue };
enum class Direction { Output };
enum class PerformanceMode { LowLatency };
enum class SharingMode { Exclusive, Shared };
enum class AudioFormat { Float };
enum class ChannelCount { Stereo };
enum class Usage { Game };
enum class ContentType { Music };
class AudioStream;
struct AudioStreamDataCallback {
    virtual ~AudioStreamDataCallback() = default;
    virtual DataCallbackResult onAudioReady(AudioStream*, void*, int32_t) = 0;
};
struct AudioStreamErrorCallback {
    virtual ~AudioStreamErrorCallback() = default;
    virtual void onErrorAfterClose(AudioStream*, Result) = 0;
};
class AudioStream {
public:
    int getSampleRate() const { return 48000; }
    int getChannelCount() const { return 2; }
    int32_t getFramesPerBurst() const { return 192; }
    void setBufferSizeInFrames(int32_t) {}
    Result requestStart() { return Result::OK; }
    void requestStop() {}
    void close() {}
};
class AudioStreamBuilder {
public:
    AudioStreamBuilder* setDirection(Direction) { return this; }
    AudioStreamBuilder* setPerformanceMode(PerformanceMode) { return this; }
    AudioStreamBuilder* setSharingMode(SharingMode) { return this; }
    AudioStreamBuilder* setFormat(AudioFormat) { return this; }
    AudioStreamBuilder* setChannelCount(ChannelCount) { return this; }
    AudioStreamBuilder* setUsage(Usage) { return this; }
    AudioStreamBuilder* setContentType(ContentType) { return this; }
    AudioStreamBuilder* setDataCallback(AudioStreamDataCallback*) { return this; }
    AudioStreamBuilder* setErrorCallback(AudioStreamErrorCallback*) { return this; }
    Result openStream(std::shared_ptr<AudioStream>& stream) {
        stream = std::make_shared<AudioStream>();
        return Result::OK;
    }
};
}
