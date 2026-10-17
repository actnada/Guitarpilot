#pragma once
#include <oboe/Oboe.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>
#include "LatestControl.h"

class GuitarEngine : public oboe::AudioStreamDataCallback,
                     public oboe::AudioStreamErrorCallback {
public:
    static constexpr int kSampleCapacity = 135; // 118 MF + 17 Palm Mute
    static constexpr int kMaxVoices = 128;
    static constexpr int kStringCount = 6;
    static constexpr int kMaxFrets = 20;
    static constexpr int kChordCount = 37; // OPEN + 12 mayores + 12 menores + 12 séptimas
    static constexpr int kMaxCustomEvents = 128;
    static constexpr int kMaxPendingAttacks = kMaxCustomEvents * kStringCount + kMaxVoices;

    enum Layer {
        MF = 0,
        PALM_MUTE = 1
    };

    // Rol físico solicitado por un evento rítmico. La máscara final siempre
    // se intersecta con la forma real del acorde (fret < 0 = cuerda muteada).
    enum StringTarget {
        ALL_ALLOWED = 0,
        BASS_3,
        TREBLE_3,
        TREBLE_4,
        ROOT_BASS,
        STRING_MASK // Máscara de seis bits en el parámetro stringNumbers
    };

    enum CustomEventType {
        EVENT_STRUM_DOWN = 0,
        EVENT_STRUM_UP = 1,
        EVENT_ROOT_BASS = 2,
        EVENT_ALT_BASS = 3,
        EVENT_PLUCK_STRING = 4,
        EVENT_RELEASE_TARGET = 5
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

    bool setCustomPattern(
        int ticksPerBar,
        const int* ticks,
        const int* offsetsMs,
        const int* types,
        const int* targets,
        const int* layers,
        const int* stringNumbers,
        const float* velocities,
        const float* spreadsMs,
        const int* releaseMs,
        const int* humanizeFlags,
        int count);
    void clearCustomPattern();

    void manualStrumDown();
    void manualStrumUp();
    void playDiagnosticSample();

    oboe::DataCallbackResult onAudioReady(
        oboe::AudioStream*, void*, int32_t) override;
    void onErrorAfterClose(oboe::AudioStream*, oboe::Result) override;

private:
    friend struct GuitarEngineTestAccess;

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
        bool bypassMaster = false;

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

    struct CustomRhythmEvent {
        int tick = 0;
        int offsetMs = 0;
        int type = EVENT_STRUM_DOWN;
        StringTarget target = ALL_ALLOWED;
        int layer = MF;
        int stringNumber = 1;
        uint8_t stringMask = 0; // bit 0: cuerda 1; bit 5: cuerda 6
        float velocity = 0.8f;
        float spreadMs = 13.0f;
        int releaseMs = 20;
        bool humanize = false;
    };

    enum class TransportReset { Start, Immediate, Stop };

    struct ControlState {
        bool playing = false;
        int pattern = 2;
        bool custom = false;
        int count = 0;
        int ticksPerBar = 16;
        std::array<CustomRhythmEvent, kMaxCustomEvents> events{};
        uint64_t patternVersion = 0;
        uint64_t resetVersion = 0;
        TransportReset reset = TransportReset::Stop;
    };

    struct PendingAttack {
        int sampleIndex;
        int physicalString;
        int64_t frame;
        uint64_t order;
        float gain;
        bool bypassMaster;
    };

    struct LaterAttack {
        bool operator()(const PendingAttack& a, const PendingAttack& b) const {
            return a.frame > b.frame || (a.frame == b.frame && a.order > b.order);
        }
    };

    struct PendingRelease {
        StringTarget target;
        int fadeMs;
        int64_t frame;
        uint64_t order;
    };

    struct LaterRelease {
        bool operator()(const PendingRelease& a, const PendingRelease& b) const {
            return a.frame > b.frame || (a.frame == b.frame && a.order > b.order);
        }
    };

    void applyPendingControl();
    void processPendingAttacks();
    void processPendingReleases();
    void activateAttack(const PendingAttack& attack);
    void fadeVoice(Voice& voice, int fadeMs);

    void resetTransport();
    int64_t framesPerSixteenth() const;

    int sampleForStringAndFret(int physicalString, int fret, int layer) const;
    int sampleWithFallback(int physicalString, int fret, int preferredLayer) const;

    void applyPendingChordChange();
    int64_t framesForPimamiGap(int stepIndex) const;
    int64_t framesForMidiTicks(int midiTicks, int ppq = 480) const;
    void triggerPimamiEvent(int stepIndex, int64_t eventFrame);
    void releaseString(int physicalString, int fadeMs);
    void releaseTargetStrings(StringTarget target, int fadeMs);
    void queueTargetRelease(StringTarget target, int fadeMs, int64_t frame);
    void cancelPendingAttacksOnString(int physicalString);

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
                            double totalSpreadMs = 20.0,
                            uint8_t stringMask = 0);

    // Laboratorio de humanización inspirado en las ideas observadas en
    // bibliotecas SFZ: variación de micro-timing, spread y ganancia,
    // sin modificar el reloj principal del patrón.
    void triggerHumanizedTargetStrum(bool down, int64_t startFrame,
                                     float velocity, StringTarget target,
                                     int preferredLayer = MF,
                                     double baseTotalSpreadMs = 20.0,
                                     uint8_t stringMask = 0);

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
    void triggerCustomPatternEvent(int tick, int64_t tickFrame);

    void startVoice(int sampleIndex, int physicalString,
                    int64_t startFrame, float gain,
                    bool bypassMaster = false);

    float renderVoice(Voice& voice);

    std::shared_ptr<oboe::AudioStream> mStream;

    std::array<Sample, kSampleCapacity> mSamples;
    std::array<Voice, kMaxVoices> mVoices;
    std::array<PendingAttack, kMaxPendingAttacks> mPendingAttacks{};
    int mPendingAttackCount = 0;
    uint64_t mAttackOrder = 0;
    std::array<PendingRelease, kMaxCustomEvents> mPendingReleases{};
    int mPendingReleaseCount = 0;
    uint64_t mReleaseOrder = 0;

    LatestControl<ControlState> mControlMailbox;
    ControlState mAudioControl{};
    uint64_t mAppliedPatternVersion = 0;
    uint64_t mAppliedResetVersion = 0;

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
    std::atomic<int> mPattern{2};
    std::atomic<float> mMasterVolume{0.85f};

    std::array<CustomRhythmEvent, kMaxCustomEvents> mCustomEvents{};
    std::atomic<int> mCustomEventCount{0};
    std::atomic<int> mCustomTicksPerBar{16};
    std::atomic<bool> mCustomPatternActive{false};

    std::atomic<bool> mPendingManualDown{false};
    std::atomic<bool> mPendingManualUp{false};
    std::atomic<bool> mPendingDiagnosticSample{false};

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

    // Biblioteca de acordes:
    // 0 = OPEN
    // 1..12  = mayores: C, C#, D, D#, E, F, F#, G, G#, A, A#, B
    // 13..24 = menores: Cm ... Bm
    // 25..36 = séptimas dominantes: C7 ... B7
    //
    // Orden de fret: cuerdas físicas 6,5,4,3,2,1.
    // Las digitaciones usan posiciones abiertas o cejillas comunes.
    // rootBass/altBass alimentan ritmos de bajo inteligente.
    const std::array<Chord, kChordCount> mChords{{
        {{{ 0, 0, 0, 0, 0, 0}}, 6,0, 5,0}, // 0 OPEN

        // Mayores
        {{{-1, 3, 2, 0, 1, 0}}, 5,3, 4,2}, // 1 C
        {{{-1, 4, 6, 6, 6, 4}}, 5,4, 4,6}, // 2 C#
        {{{-1,-1, 0, 2, 3, 2}}, 4,0, 5,0}, // 3 D
        {{{-1, 6, 8, 8, 8, 6}}, 5,6, 4,8}, // 4 D#
        {{{ 0, 2, 2, 1, 0, 0}}, 6,0, 5,2}, // 5 E
        {{{ 1, 3, 3, 2, 1, 1}}, 6,1, 5,3}, // 6 F
        {{{ 2, 4, 4, 3, 2, 2}}, 6,2, 5,4}, // 7 F#
        {{{ 3, 2, 0, 0, 0, 3}}, 6,3, 4,0}, // 8 G
        {{{ 4, 6, 6, 5, 4, 4}}, 6,4, 5,6}, // 9 G#
        {{{-1, 0, 2, 2, 2, 0}}, 5,0, 4,2}, // 10 A
        {{{-1, 1, 3, 3, 3, 1}}, 5,1, 4,3}, // 11 A#
        {{{-1, 2, 4, 4, 4, 2}}, 5,2, 4,4}, // 12 B

        // Menores
        {{{-1, 3, 5, 5, 4, 3}}, 5,3, 4,5}, // 13 Cm
        {{{-1, 4, 6, 6, 5, 4}}, 5,4, 4,6}, // 14 C#m
        {{{-1,-1, 0, 2, 3, 1}}, 4,0, 5,0}, // 15 Dm
        {{{-1, 6, 8, 8, 7, 6}}, 5,6, 4,8}, // 16 D#m
        {{{ 0, 2, 2, 0, 0, 0}}, 6,0, 5,2}, // 17 Em
        {{{ 1, 3, 3, 1, 1, 1}}, 6,1, 5,3}, // 18 Fm
        {{{ 2, 4, 4, 2, 2, 2}}, 6,2, 5,4}, // 19 F#m
        {{{ 3, 5, 5, 3, 3, 3}}, 6,3, 5,5}, // 20 Gm
        {{{ 4, 6, 6, 4, 4, 4}}, 6,4, 5,6}, // 21 G#m
        {{{-1, 0, 2, 2, 1, 0}}, 5,0, 4,2}, // 22 Am
        {{{-1, 1, 3, 3, 2, 1}}, 5,1, 4,3}, // 23 A#m
        {{{-1, 2, 4, 4, 3, 2}}, 5,2, 4,4}, // 24 Bm

        // Séptimas dominantes
        {{{-1, 3, 2, 3, 1, 0}}, 5,3, 4,2}, // 25 C7
        {{{-1, 4, 6, 4, 6, 4}}, 5,4, 4,6}, // 26 C#7
        {{{-1,-1, 0, 2, 1, 2}}, 4,0, 5,0}, // 27 D7
        {{{-1, 6, 8, 6, 8, 6}}, 5,6, 4,8}, // 28 D#7
        {{{ 0, 2, 0, 1, 0, 0}}, 6,0, 5,2}, // 29 E7
        {{{ 1, 3, 1, 2, 1, 1}}, 6,1, 5,3}, // 30 F7
        {{{ 2, 4, 2, 3, 2, 2}}, 6,2, 5,4}, // 31 F#7
        {{{ 3, 2, 0, 0, 0, 1}}, 6,3, 4,0}, // 32 G7
        {{{ 4, 6, 4, 5, 4, 4}}, 6,4, 5,6}, // 33 G#7
        {{{-1, 0, 2, 0, 2, 0}}, 5,0, 4,2}, // 34 A7
        {{{-1, 1, 3, 1, 3, 1}}, 5,1, 4,3}, // 35 A#7
        {{{-1, 2, 4, 2, 4, 2}}, 5,2, 4,4}  // 36 B7
    }};

};
