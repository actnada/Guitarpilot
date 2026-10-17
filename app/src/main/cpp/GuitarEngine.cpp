#include "GuitarEngine.h"
#include <algorithm>
#include <cmath>

bool GuitarEngine::open() {
    for (auto& s : mMfMap) s.fill(-1);
    for (auto& s : mPalmMuteMap) s.fill(-1);

    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
           ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
           ->setSharingMode(oboe::SharingMode::Exclusive)
           ->setFormat(oboe::AudioFormat::Float)
           ->setChannelCount(oboe::ChannelCount::Stereo)
           ->setUsage(oboe::Usage::Game)
           ->setContentType(oboe::ContentType::Music)
           ->setDataCallback(this)
           ->setErrorCallback(this);

    auto result = builder.openStream(mStream);
    if (result != oboe::Result::OK) {
        builder.setSharingMode(oboe::SharingMode::Shared);
        result = builder.openStream(mStream);
    }

    if (result != oboe::Result::OK || !mStream) return false;

    mOutputSampleRate = mStream->getSampleRate();
    mChannelCount = mStream->getChannelCount();

    const int32_t burst = mStream->getFramesPerBurst();
    if (burst > 0) mStream->setBufferSizeInFrames(burst * 2);

    resetTransport();
    return mStream->requestStart() == oboe::Result::OK;
}

void GuitarEngine::close() {
    mPlaying.store(false);

    if (mStream) {
        mStream->requestStop();
        mStream->close();
        mStream.reset();
    }
}

void GuitarEngine::loadSample(
    int index,
    int stringNumber,
    int fret,
    int layer,
    const int16_t* data,
    int length,
    int sampleRate) {

    if (index < 0 || index >= kSampleCapacity) return;
    if (stringNumber < 1 || stringNumber > 6) return;
    if (fret < 0 || fret >= kMaxFrets) return;
    if (layer != MF && layer != PALM_MUTE) return;
    if (!data || length <= 0) return;

    auto& sample = mSamples[index];
    sample.sampleRate = sampleRate > 0 ? sampleRate : 44100;
    sample.pcm.resize(length);

    constexpr float scale = 1.0f / 32768.0f;

    for (int i = 0; i < length; ++i) {
        sample.pcm[i] = static_cast<float>(data[i]) * scale;
    }

    if (layer == MF) {
        mMfMap[stringNumber - 1][fret] = index;
    } else {
        mPalmMuteMap[stringNumber - 1][fret] = index;
    }
}

void GuitarEngine::play() {
    mControlMailbox.update([](ControlState& state) {
        state.playing = true;
        state.reset = TransportReset::Start;
        ++state.resetVersion;
    });
}

void GuitarEngine::pause() {
    // Se detiene el scheduler, no las colas.
    mControlMailbox.update([](ControlState& state) { state.playing = false; });
}

void GuitarEngine::stop() {
    mControlMailbox.update([](ControlState& state) {
        state.playing = false;
        state.reset = TransportReset::Stop;
        ++state.resetVersion;
    });
}

void GuitarEngine::setChord(int chord) {
    // El productor solicita el acorde; PIMAMI lo adopta en el próximo P,
    // sin adelantar la fase ni detener las colas.
    mChord.store(std::clamp(chord, 0, kChordCount - 1));
}

std::array<int, 5> GuitarEngine::chordDebugState() const {
    return {{
        mChord.load(std::memory_order_relaxed),
        mPublishedAppliedChord.load(std::memory_order_relaxed),
        mPimamiChordAtLastBass.load(std::memory_order_relaxed),
        mPimamiBassCount.load(std::memory_order_relaxed),
        mPimamiBassSample.load(std::memory_order_relaxed)
    }};
}

void GuitarEngine::setBpm(int bpm) {
    mBpm.store(std::clamp(bpm, 40, 220));
}

void GuitarEngine::setStringDelayMs(int ms) {
    mStringDelayMs.store(std::clamp(ms, 1, 60));
}

void GuitarEngine::setPattern(int pattern) {
    // PIMAMI es el único patrón nativo; Balada 02 usa eventos JSON.
    (void)pattern;
    const int value = 2;
    mControlMailbox.update([value](ControlState& state) {
        state.custom = false;
        state.pattern = value;
        ++state.patternVersion;
        state.reset = TransportReset::Immediate;
        ++state.resetVersion;
    });
}

bool GuitarEngine::setCustomPattern(
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
    int count) {

    if (!ticks || !offsetsMs || !types || !targets || !layers ||
        !stringNumbers || !velocities || !spreadsMs || !releaseMs ||
        !humanizeFlags) {
        return false;
    }

    if (ticksPerBar < 1 || ticksPerBar > 64) return false;
    if (count < 1 || count > kMaxCustomEvents) return false;

    mControlMailbox.update([&](ControlState& state) {
        for (int i = 0; i < count; ++i) {
            CustomRhythmEvent event;
            event.tick = std::clamp(ticks[i], 0, ticksPerBar - 1);
            event.offsetMs = std::clamp(offsetsMs[i], -100, 100);
            event.type = std::clamp(types[i],
                static_cast<int>(EVENT_STRUM_DOWN),
                static_cast<int>(EVENT_RELEASE_TARGET));
            event.target = static_cast<StringTarget>(
                std::clamp(targets[i],
                    static_cast<int>(ALL_ALLOWED),
                    static_cast<int>(STRING_MASK)));
            event.layer = (layers[i] == PALM_MUTE) ? PALM_MUTE : MF;
            if (event.target == STRING_MASK &&
                (event.type == EVENT_STRUM_DOWN || event.type == EVENT_STRUM_UP)) {
                event.stringMask = static_cast<uint8_t>(
                    std::clamp(stringNumbers[i], 1, 63));
            } else {
                event.stringNumber = std::clamp(stringNumbers[i], 1, 6);
            }
            event.velocity = std::clamp(velocities[i], 0.0f, 1.0f);
            event.spreadMs = std::clamp(spreadsMs[i], 0.0f, 60.0f);
            event.releaseMs = std::clamp(releaseMs[i], 1, 250);
            event.humanize = humanizeFlags[i] != 0;
            state.events[i] = event;
        }
        state.ticksPerBar = ticksPerBar;
        state.count = count;
        state.custom = true;
        ++state.patternVersion;
        state.reset = TransportReset::Immediate;
        ++state.resetVersion;
    });
    return true;
}

void GuitarEngine::clearCustomPattern() {
    mControlMailbox.update([](ControlState& state) {
        state.custom = false;
        state.count = 0;
        ++state.patternVersion;
        state.reset = TransportReset::Immediate;
        ++state.resetVersion;
    });
}

void GuitarEngine::applyPendingControl() {
    if (!mControlMailbox.consume(mAudioControl)) return;

    if (mAppliedPatternVersion != mAudioControl.patternVersion) {
        mCustomEvents = mAudioControl.events;
        mCustomEventCount.store(mAudioControl.count);
        mCustomTicksPerBar.store(mAudioControl.ticksPerBar);
        mCustomPatternActive.store(mAudioControl.custom);
        mPattern.store(mAudioControl.pattern);
        mAppliedPatternVersion = mAudioControl.patternVersion;
    }

    if (mAppliedResetVersion != mAudioControl.resetVersion) {
        mPendingAttackCount = 0;
        mPendingReleaseCount = 0;
        mTickIndex = 0;
        switch (mAudioControl.reset) {
            case TransportReset::Start: resetTransport(); break;
            case TransportReset::Immediate: mNextTickFrame = mFrameCounter + 1; break;
            case TransportReset::Stop: mNextTickFrame = mFrameCounter; break;
        }
        mAppliedResetVersion = mAudioControl.resetVersion;
    }
    mPlaying.store(mAudioControl.playing);
}

void GuitarEngine::setMasterVolume(float volume) {
    mMasterVolume.store(std::clamp(volume, 0.0f, 1.0f));
}

void GuitarEngine::manualStrumDown() {
    mPendingManualDown.store(true);
}

void GuitarEngine::manualStrumUp() {
    mPendingManualUp.store(true);
}

void GuitarEngine::playDiagnosticSample() {
    mPendingDiagnosticSample.store(true);
}

void GuitarEngine::resetTransport() {
    mTickIndex = 0;
    mNextTickFrame =
        mFrameCounter + static_cast<int64_t>(mOutputSampleRate * 0.06);
}

int64_t GuitarEngine::framesPerSixteenth() const {
    const int bpm = std::max(1, mBpm.load());

    return std::max<int64_t>(
        1,
        static_cast<int64_t>(
            std::llround((60.0 / bpm / 4.0) * mOutputSampleRate)));
}

int64_t GuitarEngine::framesForMidiTicks(
    int midiTicks,
    int ppq) const {

    const int bpm = std::max(1, mBpm.load());
    const int safePpq = std::max(1, ppq);
    const int safeTicks = std::max(0, midiTicks);

    // backing-tracks usa 480 PPQ.
    // segundos = ticks * (60 / BPM) / PPQ
    const double seconds =
        static_cast<double>(safeTicks) *
        (60.0 / static_cast<double>(bpm)) /
        static_cast<double>(safePpq);

    return std::max<int64_t>(
        0,
        static_cast<int64_t>(
            std::llround(seconds * mOutputSampleRate)));
}

int64_t GuitarEngine::framesForPimamiGap(int stepIndex) const {
    // Una corchea base = 60/BPM/2.
    // El PIMAMI tiene su propio "feel": cada separación puede variar.
    const int bpm = std::max(1, mBpm.load());
    const double eighthSeconds = 60.0 / static_cast<double>(bpm) / 2.0;

    const int safeIndex = std::clamp(stepIndex, 0, 5);
    const double seconds =
        eighthSeconds * static_cast<double>(mPimamiGapScale[safeIndex]);

    return std::max<int64_t>(
        1,
        static_cast<int64_t>(
            std::llround(seconds * mOutputSampleRate)));
}

void GuitarEngine::triggerPimamiEvent(
    int stepIndex,
    int64_t eventFrame) {

    // P I M A M I
    switch (stepIndex) {
        case 0:
            // P: el bajo abre el ciclo y lleva un poco más de peso.
            triggerRootBass(eventFrame, 0.90f);
            break;

        case 1:
            // I -> cuerda 3
            triggerChordString(3, eventFrame, 0.60f, MF);
            break;

        case 2:
            // M -> cuerda 2
            triggerChordString(2, eventFrame, 0.63f, MF);
            break;

        case 3:
            // A -> cuerda 1
            triggerChordString(1, eventFrame, 0.67f, MF);
            break;

        case 4:
            // M -> cuerda 2
            triggerChordString(2, eventFrame, 0.61f, MF);
            break;

        case 5:
            // I -> cuerda 3
            triggerChordString(3, eventFrame, 0.59f, MF);
            break;

        default:
            break;
    }
}

int GuitarEngine::sampleForStringAndFret(
    int physicalString,
    int fret,
    int layer) const {

    if (physicalString < 1 || physicalString > 6) return -1;
    if (fret < 0 || fret >= kMaxFrets) return -1;

    if (layer == PALM_MUTE) {
        return mPalmMuteMap[physicalString - 1][fret];
    }

    return mMfMap[physicalString - 1][fret];
}

int GuitarEngine::sampleWithFallback(
    int physicalString,
    int fret,
    int preferredLayer) const {

    int sampleIndex =
        sampleForStringAndFret(physicalString, fret, preferredLayer);

    if (sampleIndex >= 0) return sampleIndex;

    // Si falta una articulación Palm Mute concreta,
    // caemos al MF normal para no dejar un hueco.
    if (preferredLayer == PALM_MUTE) {
        return sampleForStringAndFret(physicalString, fret, MF);
    }

    return -1;
}

void GuitarEngine::cancelPendingAttacksOnString(int physicalString) {
    auto end = std::remove_if(mPendingAttacks.begin(),
                             mPendingAttacks.begin() + mPendingAttackCount,
        [physicalString](const PendingAttack& attack) {
            return attack.physicalString == physicalString;
        });
    mPendingAttackCount = static_cast<int>(end - mPendingAttacks.begin());
    std::make_heap(mPendingAttacks.begin(), end, LaterAttack{});
}

void GuitarEngine::fadeVoice(Voice& voice, int fadeMs) {
    const int32_t fadeFrames = std::max<int32_t>(
        1,
        static_cast<int32_t>(
            (fadeMs / 1000.0) * mOutputSampleRate));

    // No alargamos una liberación que ya está en curso.
    if (voice.releaseFramesLeft <= 0 || fadeFrames < voice.releaseFramesLeft) {
        voice.releaseFramesTotal = fadeFrames;
        voice.releaseFramesLeft = fadeFrames;
    }
}

void GuitarEngine::releaseString(int physicalString, int fadeMs) {
    cancelPendingAttacksOnString(physicalString);
    for (auto& voice : mVoices) {
        if (!voice.active) continue;
        if (voice.physicalString != physicalString) continue;
        fadeVoice(voice, fadeMs);
    }
}

void GuitarEngine::releaseTargetStrings(StringTarget target, int fadeMs) {
    const auto& chord =
        mChords[std::clamp(mAppliedChord, 0, kChordCount - 1)];

    if (target == ROOT_BASS) {
        releaseString(chord.rootBassString, fadeMs);
        return;
    }

    for (int chordPos = 0; chordPos < 6; ++chordPos) {
        const int physicalString = 6 - chordPos;
        if (chord.fret[chordPos] < 0) continue;

        bool targetAllows = false;
        switch (target) {
            case ALL_ALLOWED:
                targetAllows = true;
                break;
            case BASS_3:
                targetAllows = physicalString >= 4;
                break;
            case TREBLE_3:
                targetAllows = physicalString <= 3;
                break;
            case TREBLE_4:
                targetAllows = physicalString <= 4;
                break;
            case ROOT_BASS:
            case STRING_MASK:
                break;
        }

        if (targetAllows) releaseString(physicalString, fadeMs);
    }
}

void GuitarEngine::queueTargetRelease(
    StringTarget target,
    int fadeMs,
    int64_t frame) {

    if (mPendingReleaseCount == kMaxCustomEvents) return;
    mPendingReleases[mPendingReleaseCount++] = {
        target,
        std::clamp(fadeMs, 1, 250),
        std::max(frame, mFrameCounter),
        mReleaseOrder++};
    std::push_heap(
        mPendingReleases.begin(),
        mPendingReleases.begin() + mPendingReleaseCount,
        LaterRelease{});
}

void GuitarEngine::applyPendingChordChange() {
    const int requestedChord = std::clamp(mChord.load(), 0, kChordCount - 1);
    if (requestedChord == mAppliedChord) return;

    const auto& oldChord = mChords[mAppliedChord].fret;
    const auto& newChord = mChords[requestedChord].fret;

    // Misma política que la rama testing, adaptada al scheduler con
    // PendingAttack de version-b. El acorde seleccionado nunca reinicia
    // el transporte y las notas activas permanecen sonando cuando sea posible.
    for (int chordPos = 0; chordPos < 6; ++chordPos) {
        const int oldFret = oldChord[chordPos];
        const int newFret = newChord[chordPos];
        const int physicalString = 6 - chordPos;
        if (oldFret == newFret) continue;

        if (newFret < 0) {
            // En testing, una cuerda que el nuevo acorde no permite se
            // silencia suavemente. releaseString también elimina los
            // ataques pendientes de esa cuerda.
            releaseString(physicalString, 20);
        } else {
            // El traste cambia pero la cuerda sigue permitida:
            // conservar la resonancia de su voz activa SIN aplicar fade.
            // Solo se cancelan los ataques pendientes calculados con el
            // acorde anterior. El siguiente evento usará el nuevo traste.
            cancelPendingAttacksOnString(physicalString);
        }
    }

    mAppliedChord = requestedChord;
    mPublishedAppliedChord.store(requestedChord, std::memory_order_relaxed);
}

void GuitarEngine::triggerStrum(
    bool down,
    int64_t startFrame,
    float velocity,
    bool trebleOnly,
    int preferredLayer) {

    const int64_t delayFrames =
        static_cast<int64_t>(
            (mStringDelayMs.load() / 1000.0) * mOutputSampleRate);

    triggerStrumWithDelay(
        down,
        startFrame,
        velocity,
        trebleOnly,
        preferredLayer,
        delayFrames);
}

void GuitarEngine::triggerStrumWithDelay(
    bool down,
    int64_t startFrame,
    float velocity,
    bool trebleOnly,
    int preferredLayer,
    int64_t delayFrames) {

    const auto& chord =
        mChords[std::clamp(mAppliedChord, 0, kChordCount - 1)].fret;

    struct Hit {
        int physicalString;
        int fret;
    };

    std::array<Hit, 6> hits{};
    int count = 0;

    for (int chordPos = 0; chordPos < 6; ++chordPos) {
        const int physicalString = 6 - chordPos;
        const int fret = chord[chordPos];

        if (fret < 0) continue;
        if (trebleOnly && physicalString > 3) continue;

        hits[count++] = {physicalString, fret};
    }

    const int64_t safeDelayFrames =
        std::max<int64_t>(0, delayFrames);

    for (int n = 0; n < count; ++n) {
        const int pos = down ? n : (count - 1 - n);
        const auto& hit = hits[pos];

        const int sampleIndex =
            sampleWithFallback(
                hit.physicalString,
                hit.fret,
                preferredLayer);

        if (sampleIndex >= 0) {
            startVoice(
                sampleIndex,
                hit.physicalString,
                startFrame +
                    static_cast<int64_t>(n) * safeDelayFrames,
                velocity);
        }
    }
}

void GuitarEngine::triggerChordBoxReferenceStrum(
    bool down,
    int64_t startFrame,
    int baseVelocityMidi,
    bool totalSpread) {

    const auto& chord =
        mChords[std::clamp(mAppliedChord, 0, kChordCount - 1)].fret;

    struct Hit {
        int physicalString;
        int fret;
    };

    std::array<Hit, 6> hits{};
    int count = 0;

    // chordPos 0..5 = cuerdas 6..1. La propia forma del acorde
    // funciona como máscara física: los fret < 0 nunca se disparan.
    for (int chordPos = 0; chordPos < 6; ++chordPos) {
        const int physicalString = 6 - chordPos;
        const int fret = chord[chordPos];
        if (fret < 0) continue;
        hits[count++] = {physicalString, fret};
    }

    if (count <= 0) return;

    int64_t delayFrames = 0;
    if (count > 1) {
        const double delayMs = totalSpread
            ? (20.0 / static_cast<double>(count - 1))
            : 13.0;
        delayFrames = static_cast<int64_t>(
            std::llround((delayMs / 1000.0) * mOutputSampleRate));
    }

    for (int n = 0; n < count; ++n) {
        const int pos = down ? n : (count - 1 - n);
        const auto& hit = hits[pos];

        const int sampleIndex = sampleWithFallback(
            hit.physicalString, hit.fret, MF);
        if (sampleIndex < 0) continue;

        // ChordBox: primera nota más fuerte y -3 MIDI velocity
        // por cada nota posterior. Mantenemos la misma curva en
        // ambos métodos para que el A/B compare SOLO el spread.
        const int midiVel = std::clamp(baseVelocityMidi - n * 3, 40, 127);
        const float gain = static_cast<float>(midiVel) / 127.0f;

        startVoice(
            sampleIndex,
            hit.physicalString,
            startFrame + static_cast<int64_t>(n) * delayFrames,
            gain);
    }
}

void GuitarEngine::triggerTargetStrum(
    bool down,
    int64_t startFrame,
    float velocity,
    StringTarget target,
    int preferredLayer,
    double totalSpreadMs,
    uint8_t stringMask) {

    const auto& chord =
        mChords[std::clamp(mAppliedChord, 0, kChordCount - 1)];

    // ROOT_BASS no es una zona aproximada: resuelve directamente la
    // cuerda/traste raíz declarada por el acorde. Para C es SIEMPRE
    // cuerda 5, traste 3 (C3). Esto evita confundir BASS_3 con bajo raíz.
    if (target == ROOT_BASS) {
        triggerPluckString(
            chord.rootBassString,
            chord.rootBassFret,
            startFrame,
            velocity,
            preferredLayer);
        return;
    }

    struct Hit {
        int physicalString;
        int fret;
    };

    std::array<Hit, 6> hits{};
    int count = 0;

    for (int chordPos = 0; chordPos < 6; ++chordPos) {
        const int physicalString = 6 - chordPos;
        const int fret = chord.fret[chordPos];

        // Primera máscara: forma física del acorde.
        if (fret < 0) continue;

        // Segunda máscara: zona solicitada por el evento rítmico.
        bool targetAllows = false;
        switch (target) {
            case ALL_ALLOWED:
                targetAllows = true;
                break;
            case BASS_3:
                targetAllows = physicalString >= 4;
                break;
            case TREBLE_3:
                targetAllows = physicalString <= 3;
                break;
            case TREBLE_4:
                targetAllows = physicalString <= 4;
                break;
            case ROOT_BASS:
                // Resuelto arriba.
                break;
            case STRING_MASK:
                targetAllows = (stringMask & (1u << (physicalString - 1))) != 0;
                break;
        }

        if (!targetAllows) continue;
        hits[count++] = {physicalString, fret};
    }

    if (count <= 0) return;

    int64_t delayFrames = 0;
    if (count > 1) {
        const double perStringMs =
            std::max(0.0, totalSpreadMs) /
            static_cast<double>(count - 1);

        delayFrames = static_cast<int64_t>(
            std::llround((perStringMs / 1000.0) * mOutputSampleRate));
    }

    for (int n = 0; n < count; ++n) {
        const int pos = down ? n : (count - 1 - n);
        const auto& hit = hits[pos];

        const int sampleIndex = sampleWithFallback(
            hit.physicalString, hit.fret, preferredLayer);
        if (sampleIndex < 0) continue;

        // Ligera caída interna: el primer contacto lleva más energía.
        const float perStringGain = std::max(0.70f, 1.0f - 0.045f * n);

        startVoice(
            sampleIndex,
            hit.physicalString,
            startFrame + static_cast<int64_t>(n) * delayFrames,
            velocity * perStringGain);
    }
}

uint32_t GuitarEngine::nextHumanRandom() {
    // xorshift32: suficiente para microvariaciones musicales y barato
    // para el callback de audio.
    uint32_t x = mHumanRngState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    mHumanRngState = x;
    return x;
}

float GuitarEngine::nextHumanUnit() {
    // 24 bits útiles -> [0,1].
    return static_cast<float>(nextHumanRandom() & 0x00FFFFFFu) /
           static_cast<float>(0x00FFFFFFu);
}

void GuitarEngine::triggerHumanizedTargetStrum(
    bool down,
    int64_t startFrame,
    float velocity,
    StringTarget target,
    int preferredLayer,
    double baseTotalSpreadMs,
    uint8_t stringMask) {

    const auto& chord =
        mChords[std::clamp(mAppliedChord, 0, kChordCount - 1)];

    // El scheduler musical conserva el tick exacto. La interpretación
    // recibe una ventana local de 0..6 ms (centro 3 ms, jitter +/-3 ms).
    // Así no acumulamos deriva de tempo entre eventos.
    const double timingJitterMs =
        (static_cast<double>(nextHumanUnit()) * 6.0) - 3.0;
    const double localDelayMs = 3.0 + timingJitterMs;

    const int64_t localDelayFrames = static_cast<int64_t>(
        std::llround((std::max(0.0, localDelayMs) / 1000.0) *
                     mOutputSampleRate));

    const int64_t eventFrame = startFrame + localDelayFrames;

    // +/-3% de energía por golpe.
    const float eventGain =
        0.97f + nextHumanUnit() * 0.06f;

    if (target == ROOT_BASS) {
        triggerPluckString(
            chord.rootBassString,
            chord.rootBassFret,
            eventFrame,
            velocity * eventGain,
            preferredLayer);
        return;
    }

    struct Hit {
        int physicalString;
        int fret;
    };

    std::array<Hit, 6> hits{};
    int count = 0;

    for (int chordPos = 0; chordPos < 6; ++chordPos) {
        const int physicalString = 6 - chordPos;
        const int fret = chord.fret[chordPos];

        if (fret < 0) continue;

        bool targetAllows = false;
        switch (target) {
            case ALL_ALLOWED:
                targetAllows = true;
                break;
            case BASS_3:
                targetAllows = physicalString >= 4;
                break;
            case TREBLE_3:
                targetAllows = physicalString <= 3;
                break;
            case TREBLE_4:
                targetAllows = physicalString <= 4;
                break;
            case ROOT_BASS:
                break;
            case STRING_MASK:
                targetAllows = (stringMask & (1u << (physicalString - 1))) != 0;
                break;
        }

        if (!targetAllows) continue;
        hits[count++] = {physicalString, fret};
    }

    if (count <= 0) return;

    // Cada rasgueo recorre las cuerdas ligeramente distinto (+/-10%).
    const double spreadScale =
        0.90 + static_cast<double>(nextHumanUnit()) * 0.20;
    const double totalSpreadMs =
        std::max(0.0, baseTotalSpreadMs) * spreadScale;

    int64_t delayFrames = 0;
    if (count > 1) {
        const double perStringMs =
            totalSpreadMs / static_cast<double>(count - 1);

        delayFrames = static_cast<int64_t>(
            std::llround((perStringMs / 1000.0) * mOutputSampleRate));
    }

    for (int n = 0; n < count; ++n) {
        const int pos = down ? n : (count - 1 - n);
        const auto& hit = hits[pos];

        const int sampleIndex = sampleWithFallback(
            hit.physicalString, hit.fret, preferredLayer);
        if (sampleIndex < 0) continue;

        // Conserva la caída física del golpe y añade +/-3% por cuerda.
        const float directionFalloff =
            std::max(0.70f, 1.0f - 0.045f * n);
        const float stringVariation =
            0.97f + nextHumanUnit() * 0.06f;

        startVoice(
            sampleIndex,
            hit.physicalString,
            eventFrame + static_cast<int64_t>(n) * delayFrames,
            velocity * eventGain * directionFalloff * stringVariation);
    }
}

void GuitarEngine::triggerChordWithoutRootBass(
    int64_t startFrame,
    float velocity,
    int preferredLayer) {

    const auto& chord =
        mChords[std::clamp(mAppliedChord, 0, kChordCount - 1)];

    // Golden Folk de backing-tracks:
    // en 2 y 4 dispara simultáneamente las notas superiores,
    // dejando fuera el bajo/raíz que ocupa 1 y 3.
    for (int chordPos = 0; chordPos < 6; ++chordPos) {
        const int physicalString = 6 - chordPos;
        const int fret = chord.fret[chordPos];

        if (fret < 0) continue;
        if (physicalString == chord.rootBassString) continue;

        triggerPluckString(
            physicalString,
            fret,
            startFrame,
            velocity,
            preferredLayer);
    }
}

void GuitarEngine::triggerPluckString(
    int physicalString,
    int fret,
    int64_t startFrame,
    float velocity,
    int preferredLayer) {

    const int sampleIndex =
        sampleWithFallback(
            physicalString,
            fret,
            preferredLayer);

    if (sampleIndex >= 0) {
        startVoice(
            sampleIndex,
            physicalString,
            startFrame,
            velocity);
    }
}

void GuitarEngine::triggerChordString(
    int physicalString,
    int64_t startFrame,
    float velocity,
    int preferredLayer) {

    if (physicalString < 1 || physicalString > 6) return;

    const auto& chord =
        mChords[std::clamp(mAppliedChord, 0, kChordCount - 1)].fret;

    const int chordPos = 6 - physicalString;
    const int fret = chord[chordPos];

    if (fret < 0) return;

    triggerPluckString(
        physicalString,
        fret,
        startFrame,
        velocity,
        preferredLayer);
}

void GuitarEngine::triggerRootBass(
    int64_t startFrame,
    float velocity) {

    const auto& chord =
        mChords[std::clamp(mAppliedChord, 0, kChordCount - 1)];

    triggerPluckString(
        chord.rootBassString,
        chord.rootBassFret,
        startFrame,
        velocity,
        MF);
}

void GuitarEngine::triggerAltBass(
    int64_t startFrame,
    float velocity) {

    const auto& chord =
        mChords[std::clamp(mAppliedChord, 0, kChordCount - 1)];

    triggerPluckString(
        chord.altBassString,
        chord.altBassFret,
        startFrame,
        velocity,
        MF);
}

void GuitarEngine::triggerCustomPatternEvent(
    int tick,
    int64_t tickFrame) {

    const int count = std::clamp(
        mCustomEventCount.load(), 0, kMaxCustomEvents);
    const int ticksPerBar = std::clamp(
        mCustomTicksPerBar.load(), 1, 64);
    const int64_t oneTickFrames = framesPerSixteenth();

    for (int i = 0; i < count; ++i) {
        const auto& event = mCustomEvents[i];

        int dispatchTick = event.tick;
        if (event.offsetMs < 0) {
            dispatchTick = (event.tick - 1 + ticksPerBar) % ticksPerBar;
        }

        if (dispatchTick != tick) continue;

        const int64_t offsetFrames = static_cast<int64_t>(
            std::llround(
                (static_cast<double>(event.offsetMs) / 1000.0) *
                mOutputSampleRate));

        int64_t eventFrame = tickFrame + offsetFrames;
        if (event.offsetMs < 0) {
            eventFrame = tickFrame + oneTickFrames + offsetFrames;
        }

        eventFrame = std::max<int64_t>(mFrameCounter, eventFrame);

        switch (event.type) {
            case EVENT_STRUM_DOWN:
            case EVENT_STRUM_UP: {
                const bool down = event.type == EVENT_STRUM_DOWN;
                if (event.humanize) {
                    triggerHumanizedTargetStrum(
                        down, eventFrame, event.velocity,
                        event.target, event.layer, event.spreadMs,
                        event.stringMask);
                } else {
                    triggerTargetStrum(
                        down, eventFrame, event.velocity,
                        event.target, event.layer, event.spreadMs,
                        event.stringMask);
                }
                break;
            }

            case EVENT_ROOT_BASS: {
                if (event.humanize) {
                    triggerHumanizedTargetStrum(
                        true,
                        eventFrame,
                        event.velocity,
                        ROOT_BASS,
                        event.layer,
                        0.0);
                } else {
                    const auto& chord =
                        mChords[std::clamp(
                            mAppliedChord, 0, kChordCount - 1)];
                    triggerPluckString(
                        chord.rootBassString, chord.rootBassFret,
                        eventFrame, event.velocity, event.layer);
                }
                break;
            }

            case EVENT_ALT_BASS: {
                const auto& chord =
                    mChords[std::clamp(
                        mAppliedChord, 0, kChordCount - 1)];
                triggerPluckString(
                    chord.altBassString, chord.altBassFret,
                    eventFrame, event.velocity, event.layer);
                break;
            }

            case EVENT_PLUCK_STRING:
                triggerChordString(
                    event.stringNumber, eventFrame,
                    event.velocity, event.layer);
                break;

            case EVENT_RELEASE_TARGET:
                // El evento no produce un ataque: se agenda igual que una
                // nota para respetar offsetMs y conservar el reloj estable.
                queueTargetRelease(
                    event.target, event.releaseMs, eventFrame);
                break;

            default:
                break;
        }
    }
}

void GuitarEngine::startVoice(
    int sampleIndex,
    int physicalString,
    int64_t startFrame,
    float gain,
    bool bypassMaster) {

    if (sampleIndex < 0 || sampleIndex >= kSampleCapacity) return;
    if (mSamples[sampleIndex].pcm.empty()) return;
    if (physicalString < 1 || physicalString > 6) return;

    // Solo se agenda. La cuerda que ya suena conserva su voz hasta el ataque.
    // Capacidad fija: si se satura, se descarta el ataque entrante.
    if (mPendingAttackCount == kMaxPendingAttacks) return;
    mPendingAttacks[mPendingAttackCount++] = {
        sampleIndex, physicalString, std::max(startFrame, mFrameCounter),
        mAttackOrder++, std::clamp(gain, 0.0f, 1.0f), bypassMaster};
    std::push_heap(mPendingAttacks.begin(),
                   mPendingAttacks.begin() + mPendingAttackCount, LaterAttack{});
}

void GuitarEngine::processPendingAttacks() {
    while (mPendingAttackCount > 0 && mPendingAttacks.front().frame <= mFrameCounter) {
        std::pop_heap(mPendingAttacks.begin(),
                      mPendingAttacks.begin() + mPendingAttackCount, LaterAttack{});
        const PendingAttack attack = mPendingAttacks[--mPendingAttackCount];
        activateAttack(attack);
    }
}

void GuitarEngine::processPendingReleases() {
    while (mPendingReleaseCount > 0 &&
           mPendingReleases.front().frame <= mFrameCounter) {
        std::pop_heap(
            mPendingReleases.begin(),
            mPendingReleases.begin() + mPendingReleaseCount,
            LaterRelease{});
        const PendingRelease release =
            mPendingReleases[--mPendingReleaseCount];
        releaseTargetStrings(release.target, release.fadeMs);
    }
}

void GuitarEngine::activateAttack(const PendingAttack& attack) {
    for (auto& voice : mVoices) {
        if (!voice.active || voice.physicalString != attack.physicalString) continue;
        // Dos ataques de la misma cuerda/frame: prevalece el último publicado.
        if (voice.startFrame == mFrameCounter && voice.position == 0.0) {
            voice.active = false;
        } else {
            fadeVoice(voice, 5);
        }
    }

    Voice* target = nullptr;

    for (auto& voice : mVoices) {
        if (!voice.active) {
            target = &voice;
            break;
        }
    }

    if (!target) {
        // Prototipo: si agotamos voces, reutilizamos la primera.
        target = &mVoices[0];
    }

    target->active = true;
    target->sampleIndex = attack.sampleIndex;
    target->physicalString = attack.physicalString;
    target->position = 0.0;

    target->step =
        static_cast<double>(mSamples[attack.sampleIndex].sampleRate) /
        static_cast<double>(mOutputSampleRate);

    target->startFrame = mFrameCounter;
    target->gain = attack.gain;
    target->bypassMaster = attack.bypassMaster;

    target->releaseFramesTotal = 0;
    target->releaseFramesLeft = 0;
}

float GuitarEngine::renderVoice(Voice& voice) {
    if (!voice.active) return 0.0f;
    if (mFrameCounter < voice.startFrame) return 0.0f;

    const auto& pcm = mSamples[voice.sampleIndex].pcm;
    const int i0 = static_cast<int>(voice.position);

    if (i0 >= static_cast<int>(pcm.size()) - 1) {
        voice.active = false;
        return 0.0f;
    }

    const int i1 = i0 + 1;
    const float frac =
        static_cast<float>(voice.position - i0);

    float sample =
        pcm[i0] + (pcm[i1] - pcm[i0]) * frac;

    voice.position += voice.step;

    if (voice.position >= static_cast<double>(pcm.size() - 1)) {
        voice.active = false;
    }

    float releaseGain = 1.0f;

    if (voice.releaseFramesLeft > 0 &&
        voice.releaseFramesTotal > 0) {

        releaseGain =
            static_cast<float>(voice.releaseFramesLeft) /
            static_cast<float>(voice.releaseFramesTotal);

        --voice.releaseFramesLeft;

        if (voice.releaseFramesLeft <= 0) {
            voice.active = false;
        }
    }

    return sample * voice.gain * releaseGain;
}

oboe::DataCallbackResult GuitarEngine::onAudioReady(
    oboe::AudioStream*,
    void* audioData,
    int32_t numFrames) {

    applyPendingControl();
    auto* out = static_cast<float*>(audioData);

    for (int32_t frame = 0; frame < numFrames; ++frame) {

        // Todos los patrones excepto PIMAMI cambian acorde inmediatamente.
        // PIMAMI: el acorde nuevo espera hasta tick 0 del siguiente ciclo,
        // de modo que siempre entra empezando por el pulgar/bajo.
        const bool customPattern = mCustomPatternActive.load();

        if (customPattern || mPattern.load() != 2 || !mPlaying.load()) {
            applyPendingChordChange();
        }

        // Rasgueo manual independiente del transporte.
        // En PIMAMI usa el acorde que está sonando actualmente.
        if (mPendingManualDown.exchange(false)) {
            triggerStrum(true, mFrameCounter, 0.90f, false, MF);
        }

        if (mPendingManualUp.exchange(false)) {
            triggerStrum(false, mFrameCounter, 0.78f, false, MF);
        }

        // Diagnóstico de ganancia:
        // reproduce directamente Iowa S1 fret 0 (E4), MF, gain=1.0.
        // Esta voz NO pasa por velocity de ritmo, falloff, humanización
        // ni master de GuitarPilot. Solo atraviesa el resampler/mixer Oboe.
        if (mPendingDiagnosticSample.exchange(false)) {
            const int sampleIndex = sampleForStringAndFret(1, 0, MF);
            if (sampleIndex >= 0) {
                startVoice(
                    sampleIndex,
                    1,
                    mFrameCounter,
                    1.0f,
                    true);
            }
        }

        if (mPlaying.load()) {
            while (mFrameCounter >= mNextTickFrame) {

                if (!customPattern && mPattern.load() == 2) {
                    // PIMAMI conserva el cambio EXCLUSIVAMENTE en P.
                    // Publicamos los datos del acorde y del sample elegido
                    // para distinguir un problema de control de audio de
                    // una muestra faltante, sin hacer logging en el callback.
                    if (mTickIndex == 0) {
                        applyPendingChordChange();
                        const auto& chord = mChords[mAppliedChord];
                        mPimamiChordAtLastBass.store(
                            mAppliedChord, std::memory_order_relaxed);
                        mPimamiBassSample.store(
                            sampleWithFallback(
                                chord.rootBassString, chord.rootBassFret, MF),
                            std::memory_order_relaxed);
                        mPimamiBassCount.fetch_add(
                            1, std::memory_order_relaxed);
                    }

                    triggerPimamiEvent(
                        mTickIndex,
                        mNextTickFrame);

                    const int completedStep = mTickIndex;
                    mTickIndex = (mTickIndex + 1) % 6;

                    mNextTickFrame +=
                        framesForPimamiGap(completedStep);

                } else if (customPattern) {
                    triggerCustomPatternEvent(
                        mTickIndex,
                        mNextTickFrame);

                    const int customTicks = std::clamp(
                        mCustomTicksPerBar.load(), 1, 64);

                    mTickIndex =
                        (mTickIndex + 1) % customTicks;

                    mNextTickFrame +=
                        framesPerSixteenth();
                }
            }
        }

        // Una liberación que coincide con un ataque ocurre antes de él, para
        // cortar únicamente la resonancia anterior y dejar entrar el golpe.
        processPendingReleases();
        processPendingAttacks();

        float normalMix = 0.0f;
        float diagnosticMix = 0.0f;

        for (auto& voice : mVoices) {
            const float rendered = renderVoice(voice);

            if (voice.bypassMaster) {
                diagnosticMix += rendered;
            } else {
                normalMix += rendered;
            }
        }

        float mix =
            normalMix * mMasterVolume.load() +
            diagnosticMix;

        mix = std::clamp(mix, -1.0f, 1.0f);

        const int base = frame * mChannelCount;

        for (int ch = 0; ch < mChannelCount; ++ch) {
            out[base + ch] = mix;
        }

        ++mFrameCounter;
    }

    return oboe::DataCallbackResult::Continue;
}

void GuitarEngine::onErrorAfterClose(
    oboe::AudioStream*,
    oboe::Result) {

    // Prototipo: todavía no reabrimos automáticamente.
}
