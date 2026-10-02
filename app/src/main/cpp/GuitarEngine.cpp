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
    resetTransport();
    mPlaying.store(true);
}

void GuitarEngine::pause() {
    // Se detiene el scheduler, no las colas.
    mPlaying.store(false);
}

void GuitarEngine::stop() {
    mPlaying.store(false);
    mTickIndex = 0;
    mNextTickFrame = mFrameCounter;
}

void GuitarEngine::setChord(int chord) {
    // El hilo de audio aplicará el cambio y decidirá qué cuerdas deben apagarse.
    mChord.store(std::clamp(chord, 0, kChordCount - 1));
}

void GuitarEngine::setBpm(int bpm) {
    mBpm.store(std::clamp(bpm, 40, 220));
}

void GuitarEngine::setStringDelayMs(int ms) {
    mStringDelayMs.store(std::clamp(ms, 1, 60));
}

void GuitarEngine::setPattern(int pattern) {
    const int value = std::clamp(pattern, 0, 13);
    mPattern.store(value);

    // Para esta etapa de prototipo reiniciamos la fase del patrón al cambiar
    // de estilo. Cambiar ACORDE no reinicia el transporte; cambiar de RITMO sí
    // comienza el nuevo patrón desde su primer evento.
    mTickIndex = 0;
    mNextTickFrame = mFrameCounter + 1;
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

int GuitarEngine::patternLengthTicks() const {
    // Cuadrícula base de semicorcheas para los patrones de rasgueo.
    return 16;
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

void GuitarEngine::cancelFutureVoicesOnString(int physicalString) {
    for (auto& voice : mVoices) {
        if (!voice.active) continue;
        if (voice.physicalString != physicalString) continue;

        if (voice.startFrame > mFrameCounter) {
            voice.active = false;
        }
    }
}

void GuitarEngine::releaseString(int physicalString, int fadeMs) {
    cancelFutureVoicesOnString(physicalString);

    const int32_t fadeFrames = std::max<int32_t>(
        1,
        static_cast<int32_t>(
            (fadeMs / 1000.0) * mOutputSampleRate));

    for (auto& voice : mVoices) {
        if (!voice.active) continue;
        if (voice.physicalString != physicalString) continue;
        if (voice.startFrame > mFrameCounter) continue;

        // Si ya está en release, conservamos el más corto.
        if (voice.releaseFramesLeft <= 0 ||
            fadeFrames < voice.releaseFramesLeft) {

            voice.releaseFramesTotal = fadeFrames;
            voice.releaseFramesLeft = fadeFrames;
        }
    }
}

void GuitarEngine::applyPendingChordChange() {
    const int requestedChord = std::clamp(mChord.load(), 0, kChordCount - 1);

    if (requestedChord == mAppliedChord) return;

    const auto& oldChord = mChords[mAppliedChord].fret;
    const auto& newChord = mChords[requestedChord].fret;

    // chordPos 0..5 = cuerdas físicas 6..1.
    for (int chordPos = 0; chordPos < 6; ++chordPos) {
        const int oldFret = oldChord[chordPos];
        const int newFret = newChord[chordPos];
        const int physicalString = 6 - chordPos;

        // Si la cuerda conserva exactamente el mismo traste,
        // dejamos que siga resonando.
        if (oldFret == newFret) continue;

        // Si la nueva digitación MUTEA esta cuerda, hacemos un release corto
        // de la voz ya activa. Esto evita que, por ejemplo, la 6a cuerda de
        // G/Em siga resonando debajo de C = x32010 y ensucie su bajo.
        if (newFret < 0) {
            releaseString(physicalString, 20);
            continue;
        }

        // Si la cuerda sigue permitida pero cambia de nota, conservamos la
        // estrategia v0.8/v0.10: cancelamos solo voces FUTURAS. La cola activa
        // termina de forma natural y el siguiente ataque reemplaza la cuerda.
        cancelFutureVoicesOnString(physicalString);
    }

    mAppliedChord = requestedChord;
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
    double totalSpreadMs) {

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
    double baseTotalSpreadMs) {

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

void GuitarEngine::triggerPatternEvent(
    int tick,
    int64_t tickFrame) {

    const int pattern = mPattern.load();

    if (pattern == 0) {
        // POP/ROCK: de momento todo MF.
        switch (tick) {
            case 0:  triggerStrum(true,  tickFrame, 0.92f, false, MF); break;
            case 4:  triggerStrum(true,  tickFrame, 0.72f, false, MF); break;
            case 6:  triggerStrum(false, tickFrame, 0.56f, true,  MF); break;
            case 10: triggerStrum(false, tickFrame, 0.56f, true,  MF); break;
            case 12: triggerStrum(true,  tickFrame, 0.82f, false, MF); break;
            case 14: triggerStrum(false, tickFrame, 0.62f, true,  MF); break;
            default: break;
        }
    } else if (pattern == 1) {
        // BALADA ROCK: de momento todo MF.
        switch (tick) {
            case 0:  triggerStrum(true,  tickFrame, 0.92f, false, MF); break;
            case 6:  triggerStrum(false, tickFrame, 0.42f, true,  MF); break;
            case 8:  triggerStrum(true,  tickFrame, 0.72f, false, MF); break;
            case 12: triggerStrum(true,  tickFrame, 0.55f, true,  MF); break;
            case 14: triggerStrum(false, tickFrame, 0.48f, true,  MF); break;
            default: break;
        }
    } else if (pattern == 4) {
        // GOLDEN 01 — ako/backing-tracks "strum_up_down".
        // Copia deliberadamente la lógica del proyecto de referencia:
        // - 8 corcheas por compás
        // - DOWN velocity MIDI 80
        // - UP   velocity MIDI 70
        // - 12 ticks entre notas, 480 PPQ
        // - DOWN grave->aguda / UP aguda->grave
        //
        // No añadimos jitter, treble-only ni Palm Mute para que la
        // comparación con la referencia sea limpia.
        if ((tick % 2) == 0) {
            const int eighthIndex = tick / 2;
            const bool down = (eighthIndex % 2) == 0;
            const float velocity =
                down ? (80.0f / 127.0f) : (70.0f / 127.0f);

            triggerStrumWithDelay(
                down,
                tickFrame,
                velocity,
                false,
                MF,
                framesForMidiTicks(12, 480));
        }
    } else if (pattern == 5) {
        // GOLDEN 02 — ako/backing-tracks "folk".
        // Beat 1: bajo raíz (MIDI velocity 85)
        // Beat 2: acorde superior simultáneo (70)
        // Beat 3: bajo raíz (85)
        // Beat 4: acorde superior simultáneo (70)
        switch (tick) {
            case 0:
            case 8:
                triggerRootBass(
                    tickFrame,
                    85.0f / 127.0f);
                break;

            case 4:
            case 12:
                triggerChordWithoutRootBass(
                    tickFrame,
                    70.0f / 127.0f,
                    MF);
                break;

            default:
                break;
        }
    } else if (pattern == 6 || pattern == 7) {
        // CHORDBOX FOLK — D D U - U D U -
        // Dos métodos A/B con el MISMO patrón y MISMA curva de velocity:
        // 6 = ChordBox original: 20 ms para TODO el barrido.
        // 7 = GuitarPilot histórico: 13 ms ENTRE cuerdas.
        const bool totalSpread = (pattern == 6);
        switch (tick) {
            case 0:  triggerChordBoxReferenceStrum(true,  tickFrame, 90, totalSpread); break;
            case 2:  triggerChordBoxReferenceStrum(true,  tickFrame, 90, totalSpread); break;
            case 4:  triggerChordBoxReferenceStrum(false, tickFrame, 90, totalSpread); break;
            case 8:  triggerChordBoxReferenceStrum(false, tickFrame, 90, totalSpread); break;
            case 10: triggerChordBoxReferenceStrum(true,  tickFrame, 90, totalSpread); break;
            case 12: triggerChordBoxReferenceStrum(false, tickFrame, 90, totalSpread); break;
            default: break;
        }
    } else if (pattern == 8 || pattern == 9) {
        // CHORDBOX POP BALLAD — replica EXACTA del event loop del proyecto:
        // D . . U D . . U (el patrón suma 3 beats y ChordBox lo vuelve
        // a envolver hasta completar el compás). En 4/4 esto produce
        // inicios en beats 0, 1, 1.5, 2.5 y 3.0.
        // 8 = 20 ms total; 9 = 13 ms entre cuerdas.
        const bool totalSpread = (pattern == 8);
        switch (tick) {
            case 0:  triggerChordBoxReferenceStrum(true,  tickFrame, 90, totalSpread); break;
            case 4:  triggerChordBoxReferenceStrum(false, tickFrame, 90, totalSpread); break;
            case 6:  triggerChordBoxReferenceStrum(true,  tickFrame, 90, totalSpread); break;
            case 10: triggerChordBoxReferenceStrum(false, tickFrame, 90, totalSpread); break;
            case 12: triggerChordBoxReferenceStrum(true,  tickFrame, 90, totalSpread); break;
            default: break;
        }
    } else if (pattern == 10) {
        // ZONAS DE CUERDAS — prueba de interpretación física.
        // Combina graves/agudas/acorde completo con la máscara del acorde.
        // C = x32010, por ejemplo: BASS_3 termina siendo solo cuerdas 5 y 4;
        // ROOT_BASS sigue siendo exclusivamente cuerda 5/traste 3 (C3).
        switch (tick) {
            case 0:
                triggerTargetStrum(true, tickFrame, 0.84f, BASS_3, MF, 14.0);
                break;
            case 2:
                triggerTargetStrum(false, tickFrame, 0.56f, TREBLE_3, MF, 12.0);
                break;
            case 4:
                triggerTargetStrum(true, tickFrame, 0.78f, ALL_ALLOWED, MF, 20.0);
                break;
            case 6:
                triggerTargetStrum(false, tickFrame, 0.58f, TREBLE_4, MF, 14.0);
                break;
            case 8:
                triggerTargetStrum(true, tickFrame, 0.82f, ROOT_BASS, MF, 0.0);
                break;
            case 10:
                triggerTargetStrum(false, tickFrame, 0.54f, TREBLE_3, MF, 12.0);
                break;
            case 12:
                triggerTargetStrum(true, tickFrame, 0.76f, BASS_3, MF, 14.0);
                break;
            case 14:
                triggerTargetStrum(true, tickFrame, 0.80f, ALL_ALLOWED, MF, 20.0);
                break;
            default:
                break;
        }
    } else if (pattern == 11) {
        // ZONAS HUMAN — mismo patrón de ZONAS, pero con una capa controlada
        // de interpretación inspirada en lo observado en Unreal/FSZ:
        // - reloj musical intacto;
        // - 0..6 ms de retardo local por evento (centro 3 ms);
        // - +/-10% de spread total;
        // - +/-3% de ganancia por golpe y por cuerda.
        // Sirve como A/B directo contra pattern 10.
        switch (tick) {
            case 0:
                triggerHumanizedTargetStrum(true, tickFrame, 0.84f, BASS_3, MF, 14.0);
                break;
            case 2:
                triggerHumanizedTargetStrum(false, tickFrame, 0.56f, TREBLE_3, MF, 12.0);
                break;
            case 4:
                triggerHumanizedTargetStrum(true, tickFrame, 0.78f, ALL_ALLOWED, MF, 20.0);
                break;
            case 6:
                triggerHumanizedTargetStrum(false, tickFrame, 0.58f, TREBLE_4, MF, 14.0);
                break;
            case 8:
                triggerHumanizedTargetStrum(true, tickFrame, 0.82f, ROOT_BASS, MF, 0.0);
                break;
            case 10:
                triggerHumanizedTargetStrum(false, tickFrame, 0.54f, TREBLE_3, MF, 12.0);
                break;
            case 12:
                triggerHumanizedTargetStrum(true, tickFrame, 0.76f, BASS_3, MF, 14.0);
                break;
            case 14:
                triggerHumanizedTargetStrum(true, tickFrame, 0.80f, ALL_ALLOWED, MF, 20.0);
                break;
            default:
                break;
        }
    } else if (pattern == 12) {
        // ROCK PULSE 01 — nueva lectura física:
        // AGUDAS UP -> AGUDAS DOWN -> BAJO -> pequeña espera.
        // El gesto agudo usa SOLO cuerdas 3,2,1.
        // Después del bajo queda una semicorchea de aire y el ciclo repite.
        switch (tick) {
            case 0:
            case 4:
            case 8:
            case 12:
                triggerHumanizedTargetStrum(
                    false, tickFrame, 0.50f, TREBLE_3, MF, 10.0);
                break;

            case 1:
            case 5:
            case 9:
            case 13:
                triggerHumanizedTargetStrum(
                    true, tickFrame, 0.68f, TREBLE_3, MF, 12.0);
                break;

            case 2:
            case 6:
            case 10:
            case 14:
                triggerHumanizedTargetStrum(
                    true, tickFrame, 0.84f, ROOT_BASS, MF, 0.0);
                break;

            // 3,7,11,15 = pequeña espera.
            default:
                break;
        }
    } else if (pattern == 13) {
        // LATIN BALLAD 01 — referencia de prueba: "Hojas Secas".
        // Los bajos se conservan separados. SOLO los golpes agudos alternan
        // dirección y nunca bajan de la cuerda 4:
        // DOWN -> UP -> DOWN -> UP...
        switch (tick) {
            case 0:
                triggerHumanizedTargetStrum(
                    true, tickFrame, 0.84f, ROOT_BASS, MF, 0.0);
                break;
            case 4:
                triggerHumanizedTargetStrum(
                    true, tickFrame, 0.78f, ROOT_BASS, MF, 0.0);
                break;

            // Frase aguda 1: empieza DOWN y alterna.
            case 6:
                triggerHumanizedTargetStrum(
                    true, tickFrame, 0.62f, TREBLE_4, MF, 12.0);
                break;
            case 7:
                triggerHumanizedTargetStrum(
                    false, tickFrame, 0.48f, TREBLE_4, MF, 10.0);
                break;
            case 8:
                triggerHumanizedTargetStrum(
                    true, tickFrame, 0.66f, TREBLE_4, MF, 12.0);
                break;
            case 9:
                triggerHumanizedTargetStrum(
                    false, tickFrame, 0.50f, TREBLE_4, MF, 10.0);
                break;

            // Frase aguda 2: mantiene la alternancia DOWN/UP.
            case 12:
                triggerHumanizedTargetStrum(
                    true, tickFrame, 0.60f, TREBLE_4, MF, 12.0);
                break;
            case 13:
                triggerHumanizedTargetStrum(
                    false, tickFrame, 0.46f, TREBLE_4, MF, 10.0);
                break;
            case 14:
                triggerHumanizedTargetStrum(
                    true, tickFrame, 0.68f, TREBLE_4, MF, 12.0);
                break;
            case 15:
                triggerHumanizedTargetStrum(
                    false, tickFrame, 0.50f, TREBLE_4, MF, 10.0);
                break;
            default:
                break;
        }
    } else if (pattern == 3) {
        // ROCK MUTED: prueba inicial usando articulación Palm Mute real.
        // Graves apagados alternados con rasgueos MF más abiertos.
        switch (tick) {
            case 0:
                triggerRootBass(tickFrame, 0.84f);
                break;
            case 2:
                triggerStrum(true, tickFrame, 0.72f, false, PALM_MUTE);
                break;
            case 4:
                triggerStrum(true, tickFrame, 0.82f, false, MF);
                break;
            case 6:
                triggerStrum(false, tickFrame, 0.50f, true, MF);
                break;
            case 8:
                triggerRootBass(tickFrame, 0.80f);
                break;
            case 10:
                triggerStrum(true, tickFrame, 0.70f, false, PALM_MUTE);
                break;
            case 12:
                triggerStrum(true, tickFrame, 0.84f, false, MF);
                break;
            case 14:
                triggerStrum(false, tickFrame, 0.54f, true, MF);
                break;
            default:
                break;
        }
    } else {
        // PIMAMI se agenda fuera de la cuadrícula rígida de semicorcheas.
        // Aquí no hacemos nada: el callback llama triggerPimamiEvent()
        // y usa framesForPimamiGap() para conservar su propio fraseo.
        (void)tick;
        (void)tickFrame;
    }
}

void GuitarEngine::startVoice(
    int sampleIndex,
    int physicalString,
    int64_t startFrame,
    float gain) {

    if (sampleIndex < 0 || sampleIndex >= kSampleCapacity) return;
    if (mSamples[sampleIndex].pcm.empty()) return;
    if (physicalString < 1 || physicalString > 6) return;

    // Regla física:
    // al volver a atacar la misma cuerda, la vibración anterior
    // cae suavemente en vez de acumularse indefinidamente.
    releaseString(physicalString, 5);

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
    target->sampleIndex = sampleIndex;
    target->physicalString = physicalString;
    target->position = 0.0;

    target->step =
        static_cast<double>(mSamples[sampleIndex].sampleRate) /
        static_cast<double>(mOutputSampleRate);

    target->startFrame = startFrame;
    target->gain = std::clamp(gain, 0.0f, 1.0f);

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

    auto* out = static_cast<float*>(audioData);

    for (int32_t frame = 0; frame < numFrames; ++frame) {

        // Todos los patrones excepto PIMAMI cambian acorde inmediatamente.
        // PIMAMI: el acorde nuevo espera hasta tick 0 del siguiente ciclo,
        // de modo que siempre entra empezando por el pulgar/bajo.
        if (mPattern.load() != 2 || !mPlaying.load()) {
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

        if (mPlaying.load()) {
            while (mFrameCounter >= mNextTickFrame) {

                if (mPattern.load() == 2) {
                    // PIMAMI tiene un reloj propio.
                    // El acorde pendiente solo se aplica antes del P.
                    if (mTickIndex == 0) {
                        applyPendingChordChange();
                    }

                    triggerPimamiEvent(
                        mTickIndex,
                        mNextTickFrame);

                    const int completedStep = mTickIndex;
                    mTickIndex = (mTickIndex + 1) % 6;

                    mNextTickFrame +=
                        framesForPimamiGap(completedStep);

                } else {
                    // Rasgueos, Rock Muted y Golden Rhythms usan
                    // una cuadrícula base de semicorcheas.
                    triggerPatternEvent(
                        mTickIndex,
                        mNextTickFrame);

                    mTickIndex =
                        (mTickIndex + 1) % patternLengthTicks();

                    mNextTickFrame +=
                        framesPerSixteenth();
                }
            }
        }

        float mix = 0.0f;

        for (auto& voice : mVoices) {
            mix += renderVoice(voice);
        }

        mix *= mMasterVolume.load();

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
