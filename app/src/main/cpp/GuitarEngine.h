#pragma once
#include <oboe/Oboe.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

class GuitarEngine : public oboe::AudioStreamDataCallback,
                     public oboe::AudioStreamErrorCallback {
public:
    static constexpr int kSampleCapacity = 135; // 118 MF + 17 Palm Mute
    static constexpr int kMaxVoices = 128;
    static constexpr int kStringCount = 6;
    static constexpr int kMaxFrets = 20;

    enum Layer {
        MF = 0,
        PALM_MUTE = 1
    };

    // Rol físico solicitado por un evento rítmico. La máscara final siempre
    // se intersecta con la forma real del acorde (fret < 0 = cuerda muteada).
    enum StringTarget {
        ALL_ALLOWED = 0,
        BASS_3,      // cuerdas físicas 6,5,4
        TREBLE_3,    // cuerdas físicas 3,2,1
        TREBLE_4,    // cuerdas físicas 4,3,2,1
        ROOT_BASS    // una sola cuerda: bajo raíz explícito del acorde
    };

    bool open();
    void close();

    void loadSample(int index, int stringNumber, int fret, int layer,
                    const int16_t* data, int length, int sampleRate);

    void play();
    void pause();
    void stop();

    void setChord(int chord);
    void setBpm(int bpm);
    void setStringDelayMs(int delayMs);
    void setPattern(int pattern);
    void setMasterVolume(float volume);

    void manualStrumDown();
    void manualStrumUp();

    oboe::DataCallbackResult onAudioReady(
        oboe::AudioStream*, void*, int32_t) override;
    void onErrorAfterClose(oboe::AudioStream*, oboe::Result) override;

private:
    struct Sample {
        std::vector<float> pcm;
        int sampleRate = 44100;
    };

    struct Voice {
        bool active = false;
        int sampleIndex = -1;
        int physicalString = -1; // 1..6
        double position = 0.0;
        double step = 1.0;
        int64_t startFrame = 0;
        float gain = 1.0f;

        int32_t releaseFramesTotal = 0;
        int32_t releaseFramesLeft = 0;
    };

    // Orden de trastes: cuerda 6,5,4,3,2,1.
    // -1 = cuerda muteada.
    struct Chord {
        std::array<int, 6> fret;

        int rootBassString;
        int rootBassFret;

        int altBassString;
        int altBassFret;
    };

    void resetTransport();
    int64_t framesPerSixteenth() const;

    int sampleForStringAndFret(int physicalString, int fret, int layer) const;
    int sampleWithFallback(int physicalString, int fret, int preferredLayer) const;

    void applyPendingChordChange();
    int patternLengthTicks() const;
    int64_t framesForPimamiGap(int stepIndex) const;
    int64_t framesForMidiTicks(int midiTicks, int ppq = 480) const;
    void triggerPimamiEvent(int stepIndex, int64_t eventFrame);
    void releaseString(int physicalString, int fadeMs);
    void cancelFutureVoicesOnString(int physicalString);

    void triggerStrum(bool down, int64_t startFrame,
                      float velocity = 1.0f,
                      bool trebleOnly = false,
                      int preferredLayer = MF);

    void triggerStrumWithDelay(bool down, int64_t startFrame,
                               float velocity,
                               bool trebleOnly,
                               int preferredLayer,
                               int64_t delayFrames);

    // ChordBox reference gesture: same velocity ramp, two spread models
    // for A/B comparison. totalSpread=true -> 20 ms across the whole strum;
    // false -> 13 ms between adjacent strings.
    void triggerChordBoxReferenceStrum(bool down, int64_t startFrame,
                                       int baseVelocityMidi,
                                       bool totalSpread);

    void triggerTargetStrum(bool down, int64_t startFrame,
                            float velocity, StringTarget target,
                            int preferredLayer = MF,
                            double totalSpreadMs = 20.0);

    // Laboratorio de humanización inspirado en las ideas observadas en
    // bibliotecas SFZ: variación de micro-timing, spread y ganancia,
    // sin modificar el reloj principal del patrón.
    void triggerHumanizedTargetStrum(bool down, int64_t startFrame,
                                     float velocity, StringTarget target,
                                     int preferredLayer = MF,
                                     double baseTotalSpreadMs = 20.0);

    uint32_t nextHumanRandom();
    float nextHumanUnit();

    void triggerChordWithoutRootBass(int64_t startFrame,
                                     float velocity,
                                     int preferredLayer = MF);

    void triggerPluckString(int physicalString, int fret,
                            int64_t startFrame,
                            float velocity = 1.0f,
                            int preferredLayer = MF);

    void triggerChordString(int physicalString,
                            int64_t startFrame,
                            float velocity = 1.0f,
                            int preferredLayer = MF);

    void triggerRootBass(int64_t startFrame, float velocity = 1.0f);
    void triggerAltBass(int64_t startFrame, float velocity = 1.0f);
    void triggerPatternEvent(int tick, int64_t tickFrame);

    void startVoice(int sampleIndex, int physicalString,
                    int64_t startFrame, float gain);

    float renderVoice(Voice& voice);

    std::shared_ptr<oboe::AudioStream> mStream;

    std::array<Sample, kSampleCapacity> mSamples;
    std::array<Voice, kMaxVoices> mVoices;

    // [0..5] = cuerdas físicas [1..6]
    std::array<std::array<int, kMaxFrets>, kStringCount> mMfMap{};
    std::array<std::array<int, kMaxFrets>, kStringCount> mPalmMuteMap{};

    std::atomic<bool> mPlaying{false};

    // Acorde solicitado desde Kotlin. En PIMAMI puede quedar pendiente
    // hasta el inicio del siguiente ciclo.
    std::atomic<int> mChord{0};

    // Acorde que realmente está sonando ahora.
    int mAppliedChord = 0;

    std::atomic<int> mBpm{100};
    std::atomic<int> mStringDelayMs{13};
    std::atomic<int> mPattern{0};
    std::atomic<float> mMasterVolume{0.85f};

    std::atomic<bool> mPendingManualDown{false};
    std::atomic<bool> mPendingManualUp{false};

    // PRNG ligero, usado solo por el hilo de audio para microvariaciones.
    // No usa std::random ni realiza asignaciones dentro del callback.
    uint32_t mHumanRngState = 0x6D2B79F5u;

    int32_t mOutputSampleRate = 48000;
    int32_t mChannelCount = 2;
    int64_t mFrameCounter = 0;
    int64_t mNextTickFrame = 0;
    int mTickIndex = 0;

    // PIMAMI: separación relativa respecto a una corchea.
    // A 100 BPM una corchea base son 300 ms; este perfil produce
    // aproximadamente 360, 345, 342, 342, 345 y 354 ms.
    // Guardamos cada distancia por separado para que el futuro editor
    // pueda mover cada evento de forma independiente.
    const std::array<float, 6> mPimamiGapScale{{
        1.20f, 1.15f, 1.14f, 1.14f, 1.15f, 1.18f
    }};

    // OPEN, Em, C, G, D, Bm.
    // Bajos musicales explícitos:
    // Em: E2 / B2
    // C : C3 / E3
    // G : G2 / D3
    // D : D3 / A2
    // Bm: B2 / F#3
    const std::array<Chord, 6> mChords{{
        {{{ 0, 0, 0, 0, 0, 0}}, 6,0, 5,0}, // OPEN
        {{{ 0, 2, 2, 0, 0, 0}}, 6,0, 5,2}, // Em
        {{{-1, 3, 2, 0, 1, 0}}, 5,3, 4,2}, // C
        {{{ 3, 2, 0, 0, 0, 3}}, 6,3, 4,0}, // G
        {{{-1,-1, 0, 2, 3, 2}}, 4,0, 5,0}, // D
        {{{-1, 2, 4, 4, 3, 2}}, 5,2, 4,4}  // Bm
    }};
};
