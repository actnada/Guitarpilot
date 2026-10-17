#include "GuitarEngine.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
#include <utility>

struct GuitarEngineTestAccess {
    static void schedule(GuitarEngine& e, int sample, int string, int64_t frame) {
        e.startVoice(sample, string, frame, 1.0f);
    }
    static int64_t frame(const GuitarEngine& e) { return e.mFrameCounter; }
    static int pending(const GuitarEngine& e) { return e.mPendingAttackCount; }
    static void apply(GuitarEngine& e) { e.applyPendingControl(); }
    static bool playing(const GuitarEngine& e) { return e.mPlaying.load(); }
    static int pattern(const GuitarEngine& e) { return e.mPattern.load(); }
    static int tick(const GuitarEngine& e) { return e.mTickIndex; }
    static int64_t next(const GuitarEngine& e) { return e.mNextTickFrame; }
    static bool custom(const GuitarEngine& e) { return e.mCustomPatternActive.load(); }
    static int count(const GuitarEngine& e) { return e.mCustomEventCount.load(); }
    static float velocity(const GuitarEngine& e) { return e.mCustomEvents[0].velocity; }
    static int activeChord(const GuitarEngine& e) { return e.mAppliedChord; }
    static int sampleOnString(const GuitarEngine& e, int physical) {
        int64_t newest = -1;
        int sample = -1;
        for (const auto& voice : e.mVoices) {
            if (voice.active && voice.physicalString == physical &&
                voice.startFrame >= newest) {
                newest = voice.startFrame;
                sample = voice.sampleIndex;
            }
        }
        return sample;
    }
    static bool validMinorSeventh(const GuitarEngine& e, int id) {
        if (id < 37 || id >= GuitarEngine::kChordCount) return false;
        const auto& chord = e.mChords[id];
        // Notas MIDI al aire, orden de cuerdas físicas 6..1.
        constexpr std::array<int, 6> openMidi{{40, 45, 50, 55, 59, 64}};
        std::array<bool, 12> actual{};
        const int rootPc = id - 37; // C = 0, ... B = 11.
        for (int pos = 0; pos < 6; ++pos) {
            const int fret = chord.fret[pos];
            if (fret == -1) continue;
            if (fret < 0 || fret >= GuitarEngine::kMaxFrets) return false;
            actual[(openMidi[pos] + fret) % 12] = true;
        }
        std::array<bool, 12> expected{};
        for (const int delta : {0, 3, 7, 10})
            expected[(rootPc + delta) % 12] = true;
        if (actual != expected) return false;
        for (const auto bass : {std::pair<int, int>{chord.rootBassString, chord.rootBassFret},
                                std::pair<int, int>{chord.altBassString, chord.altBassFret}}) {
            const int physical = bass.first;
            if (physical < 1 || physical > 6) return false;
            if (chord.fret[6 - physical] != bass.second) return false;
        }
        const int rootBassIndex = 6 - chord.rootBassString;
        return (openMidi[rootBassIndex] + chord.rootBassFret) % 12 == rootPc;
    }
    static std::vector<int> pendingSampleIndices(const GuitarEngine& e) {
        std::vector<int> result;
        for (int i = 0; i < e.mPendingAttackCount; ++i)
            result.push_back(e.mPendingAttacks[i].sampleIndex);
        std::sort(result.begin(), result.end());
        return result;
    }
    static uint8_t stringMask(const GuitarEngine& e) { return e.mCustomEvents[0].stringMask; }
    static std::vector<int> pendingStrings(const GuitarEngine& e) {
        std::vector<int> result;
        for (int i = 0; i < e.mPendingAttackCount; ++i)
            result.push_back(e.mPendingAttacks[i].physicalString);
        std::sort(result.begin(), result.end());
        return result;
    }
    static void triggerCustomEvent(GuitarEngine& e, int tick, int64_t frame) {
        e.triggerCustomPatternEvent(tick, frame);
    }
};

static void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
static bool near(float a, float b) { return std::abs(a - b) < 0.00001f; }

struct Fixture {
    GuitarEngine engine;
    Fixture() {
        check(engine.open(), "host I/O stub opens");
        engine.setMasterVolume(1.0f);
        std::vector<int16_t> pcm(96000, 8192);
        engine.loadSample(0, 1, 0, GuitarEngine::MF, pcm.data(), int(pcm.size()), 48000);
        std::fill(pcm.begin(), pcm.end(), int16_t{4096});
        engine.loadSample(1, 1, 1, GuitarEngine::MF, pcm.data(), int(pcm.size()), 48000);
    }
    ~Fixture() { engine.close(); }
    std::vector<float> render(int frames) {
        std::vector<float> out(size_t(frames) * 2);
        engine.onAudioReady(nullptr, out.data(), frames);
        return out;
    }
    void schedule(int sample, int64_t frame, int string = 1) {
        GuitarEngineTestAccess::schedule(engine, sample, string, frame);
    }
};

static void futureAttackKeepsPreviousNote() {
    Fixture f;
    f.schedule(0, 0);
    f.render(10);
    const int64_t start = GuitarEngineTestAccess::frame(f.engine) + 2400;
    f.schedule(1, start);
    auto before = f.render(2400);
    check(std::all_of(before.begin(), before.end(), [](float s) { return near(s, .25f); }),
          "50 ms future attack must not fade the old note early");
    auto attack = f.render(1);
    check(near(attack[0], .375f), "new attack starts at its exact frame with a fade of old note");
    f.render(240);
    check(near(f.render(1)[0], .125f), "old note is gone after 5 ms crossfade");
}

static void futureNotesSurviveAndStayChronological() {
    Fixture f;
    f.schedule(0, 400); // Enqueued out of time order, on purpose.
    f.schedule(1, 100);
    check(GuitarEngineTestAccess::pending(f.engine) == 2, "same-string future attacks coexist");
    check(near(f.render(100).back(), 0), "silence until first attack");
    check(near(f.render(1)[0], .125f), "earlier event plays even when enqueued second");
    f.render(299);
    check(near(f.render(1)[0], .375f), "later event survives the earlier reattack");
    f.render(240);
    check(near(f.render(1)[0], .25f), "chronological final note remains");
}

static void simultaneousSameStringUsesLastAttack() {
    Fixture f;
    f.schedule(0, 10);
    f.schedule(1, 10);
    f.render(10);
    check(near(f.render(1)[0], .125f), "same-frame duplicate string must not double the attack");
}

static void chordMuteCancelsPendingString() {
    Fixture f;
    f.schedule(0, 0, 6);
    f.render(1);
    f.schedule(1, 2000, 6);
    f.engine.setChord(1); // C excludes string 6.
    f.render(1000);
    check(GuitarEngineTestAccess::pending(f.engine) == 0, "chord mute cancels old pending string");
    check(near(f.render(1200).back(), 0), "muted string must not come back later");
}

static void controlChangesApplyOnAudioThread() {
    Fixture f;
    const auto originalNext = GuitarEngineTestAccess::next(f.engine);
    f.engine.play();
    f.engine.pause();
    f.engine.setPattern(13);
    check(!GuitarEngineTestAccess::playing(f.engine), "producer does not directly change playback");
    check(GuitarEngineTestAccess::next(f.engine) == originalNext, "producer does not write transport");
    GuitarEngineTestAccess::apply(f.engine);
    check(!GuitarEngineTestAccess::playing(f.engine), "latest pause is preserved across pattern change");
    check(GuitarEngineTestAccess::pattern(f.engine) == 2, "removed pattern IDs fall back to PIMAMI");
    check(GuitarEngineTestAccess::tick(f.engine) == 0, "audio thread resets tick");
    check(GuitarEngineTestAccess::next(f.engine) == 1, "audio thread schedules new pattern");
    f.schedule(0, 1000);
    f.engine.stop();
    GuitarEngineTestAccess::apply(f.engine);
    check(GuitarEngineTestAccess::pending(f.engine) == 0, "stop cancels stale pending attacks");
    check(GuitarEngineTestAccess::next(f.engine) == 0, "stop resets on audio clock");
}

static void chordChangePreservesPlayingPitchWhenStringRemainsAllowed() {
    Fixture f;
    f.schedule(0, 0); // OPEN: primera cuerda E4 todavía resonando.
    f.render(1);
    f.schedule(0, 2000); // Ataque futuro con digitación vieja.
    f.engine.setChord(3); // D: cuerda 1 cambia traste 0 -> 2, sigue permitida.
    const auto tail = f.render(1200);
    check(GuitarEngineTestAccess::pending(f.engine) == 0,
          "changed fret cancels only stale future attacks");
    check(std::all_of(tail.begin(), tail.end(), [](float sample) {
        return near(sample, .25f);
    }), "playing pitch must stay at constant amplitude across chord selection");
    check(near(f.render(1)[0], .25f),
          "old resonance remains until the next actual attack");
}

static void chordChangePreservesCommonPitch() {
    Fixture f;
    f.engine.setChord(1); // C, primera cuerda E4.
    f.render(1);
    f.schedule(0, GuitarEngineTestAccess::frame(f.engine));
    f.render(1);
    f.engine.setChord(22); // Am, conserva la misma primera cuerda E4.
    auto tail = f.render(1200);
    check(near(tail.back(), .25f),
          "common string and fret must keep ringing across chord changes");
}

static void customReleaseTargetRespectsItsScheduledFrame() {
    Fixture f;
    f.schedule(0, 0, 1);
    f.schedule(0, 0, 6);
    f.render(1);

    int tick=0, offset=0, type=GuitarEngine::EVENT_RELEASE_TARGET;
    int target=GuitarEngine::TREBLE_3, layer=0, string=1, release=20, human=0;
    float velocity=0, spread=0;
    check(f.engine.setCustomPattern(16,&tick,&offset,&type,&target,&layer,&string,
                                    &velocity,&spread,&release,&human,1),
          "release event pattern accepted");
    GuitarEngineTestAccess::apply(f.engine);
    const int64_t releaseFrame = GuitarEngineTestAccess::frame(f.engine) + 100;
    GuitarEngineTestAccess::triggerCustomEvent(f.engine, 0, releaseFrame);

    auto before = f.render(100);
    check(near(before.back(), .5f),
          "release event must wait until its scheduled frame");
    auto after = f.render(1000);
    check(near(after.back(), .25f),
          "release target must mute treble while keeping bass resonating");
}

static void customPatternCopiesInputAndLatestState() {
    Fixture f;
    int tick=0, offset=50, type=4, target=0, layer=0, string=1, release=20, human=0;
    float velocity=.4f, spread=13;
    check(f.engine.setCustomPattern(16,&tick,&offset,&type,&target,&layer,&string,
                                   &velocity,&spread,&release,&human,1), "valid pattern accepted");
    velocity=.9f; // JNI caller buffers can change immediately after publication.
    GuitarEngineTestAccess::apply(f.engine);
    check(GuitarEngineTestAccess::custom(f.engine), "custom mode switches as one state");
    check(GuitarEngineTestAccess::count(f.engine)==1, "custom event count matches payload");
    check(near(GuitarEngineTestAccess::velocity(f.engine),.4f), "pattern owns a copy of input arrays");
    f.engine.setPattern(2);
    f.engine.clearCustomPattern();
    f.engine.play();
    GuitarEngineTestAccess::apply(f.engine);
    check(!GuitarEngineTestAccess::custom(f.engine), "latest mode wins");
    check(GuitarEngineTestAccess::pattern(f.engine)==2, "clear preserves chosen built-in pattern");
    check(GuitarEngineTestAccess::playing(f.engine), "play is not lost when publications coalesce");
    check(GuitarEngineTestAccess::next(f.engine)==2880, "play retains 60 ms lead-in");
}

static void maskedStrumSupportsFourBarTicksAndExactStrings() {
    Fixture f;
    // Acorde OPEN: seis cuerdas permitidas; cargamos muestras reales por cuerda.
    std::vector<int16_t> pcm(2048, 8192);
    for (int physical = 2; physical <= 6; ++physical) {
        f.engine.loadSample(physical, physical, 0, GuitarEngine::MF,
                            pcm.data(), int(pcm.size()), 48000);
    }

    int tick=63, offset=0, type=GuitarEngine::EVENT_STRUM_UP;
    int target=GuitarEngine::STRING_MASK, layer=GuitarEngine::MF;
    int mask=(1 << 5) | (1 << 2) | (1 << 0), release=20, human=0;
    float velocity=.65f, spread=15;
    check(f.engine.setCustomPattern(64,&tick,&offset,&type,&target,&layer,
                                    &mask,&velocity,&spread,&release,&human,1),
          "64-tick multi-bar pattern accepted");
    mask = 63; // La entrada JNI puede cambiar inmediatamente después.
    GuitarEngineTestAccess::apply(f.engine);
    check(GuitarEngineTestAccess::stringMask(f.engine) == 37,
          "engine keeps an independent six-bit mask");
    GuitarEngineTestAccess::triggerCustomEvent(f.engine, 63,
        GuitarEngineTestAccess::frame(f.engine) + 120);
    check(GuitarEngineTestAccess::pendingStrings(f.engine) ==
          std::vector<int>({1, 3, 6}),
          "masked upstrum only schedules the explicitly selected strings");
}

static void customStrumUsesLatestChordWithoutRestartingRhythm() {
    Fixture f;
    // Grabaciones cargadas: S1 fret 0 (sample 0) y S1 fret 1 (sample 1).
    int tick=0, offset=0, type=GuitarEngine::EVENT_STRUM_DOWN;
    int target=GuitarEngine::TREBLE_3, layer=GuitarEngine::MF;
    int string=1, release=20, human=0;
    float velocity=.8f, spread=0;
    check(f.engine.setCustomPattern(16,&tick,&offset,&type,&target,
                                    &layer,&string,&velocity,&spread,
                                    &release,&human,1), "custom strum accepted");
    f.engine.setChord(1); // C: S1 = E4 abierto
    f.engine.play();
    f.render(1);
    check(GuitarEngineTestAccess::activeChord(f.engine) == 1,
          "C chord is active in the JSON accompaniment");

    int64_t future = GuitarEngineTestAccess::frame(f.engine) + 100;
    GuitarEngineTestAccess::triggerCustomEvent(f.engine, 0, future);
    check(GuitarEngineTestAccess::pendingSampleIndices(f.engine) ==
          std::vector<int>({0}), "C strum uses first-string fret 0");

    const int tickBefore = GuitarEngineTestAccess::tick(f.engine);
    f.engine.setChord(6); // F: S1 = F4, fret 1
    f.render(1);
    check(GuitarEngineTestAccess::activeChord(f.engine) == 6,
          "F chord applies while the rhythm continues");
    check(GuitarEngineTestAccess::tick(f.engine) == tickBefore,
          "chord changes must not restart the pattern");
    check(GuitarEngineTestAccess::pending(f.engine) == 0,
          "previous chord pending attacks get cancelled");

    future = GuitarEngineTestAccess::frame(f.engine) + 100;
    GuitarEngineTestAccess::triggerCustomEvent(f.engine, 0, future);
    check(GuitarEngineTestAccess::pendingSampleIndices(f.engine) ==
          std::vector<int>({1}), "next strum uses F chord recording");
}

static void pimamiSwitchesChordAtNextPWithoutRestarting() {
    Fixture f;
    // C (cuerda física 3, traste 0) y F (cuerda 3, traste 2).
    std::vector<int16_t> pcm(96000, 8192);
    f.engine.loadSample(2, 3, 0, GuitarEngine::MF,
                        pcm.data(), int(pcm.size()), 48000);
    f.engine.loadSample(3, 3, 2, GuitarEngine::MF,
                        pcm.data(), int(pcm.size()), 48000);

    f.engine.setChord(1); // C
    f.engine.play();
    f.render(2881); // El primer P ha ocurrido, siguiente paso I.
    check(GuitarEngineTestAccess::tick(f.engine) == 1,
          "PIMAMI must advance from P to I");
    check(GuitarEngineTestAccess::activeChord(f.engine) == 1,
          "first P of the sequence uses selected C");
    auto info = f.engine.chordDebugState();
    check(info[0] == 1 && info[1] == 1 && info[2] == 1 && info[3] == 1,
          "PIMAMI first bass publishes applied C chord");
    check(info[4] == -1, "diagnostic detects no root-bass sample in test bank");

    f.engine.setChord(6); // F; testing aplica el acorde nuevo en el próximo P.
    const auto beforeTick = GuitarEngineTestAccess::tick(f.engine);
    const auto beforeNext = GuitarEngineTestAccess::next(f.engine);
    f.render(1);
    check(GuitarEngineTestAccess::activeChord(f.engine) == 1,
          "PIMAMI pending chord must wait for next P, as in testing");
    info = f.engine.chordDebugState();
    check(info[0] == 6 && info[1] == 1 && info[2] == 1 && info[3] == 1,
          "diagnostic separates selected F from active C before the next P");
    check(GuitarEngineTestAccess::tick(f.engine) == beforeTick &&
          GuitarEngineTestAccess::next(f.engine) == beforeNext,
          "selecting a chord must not move the PIMAMI clock");

    auto advanceOne = [&f]() {
        const auto until = GuitarEngineTestAccess::next(f.engine) -
                           GuitarEngineTestAccess::frame(f.engine) + 1;
        check(until > 0 && until < 25000, "valid next PIMAMI interval");
        f.render(static_cast<int>(until));
    };

    // I-M-A-M-I siguen el acorde C hasta finalizar el ciclo.
    advanceOne();
    check(GuitarEngineTestAccess::tick(f.engine) == 2,
          "first I must play without restarting at P");
    check(GuitarEngineTestAccess::sampleOnString(f.engine, 3) == 2,
          "first I keeps the old C chord until next P");
    for (int step = 3; step <= 5; ++step) {
        advanceOne();
        check(GuitarEngineTestAccess::tick(f.engine) == step,
              "PIMAMI preserves its exact finger sequence");
    }
    advanceOne(); // Último I de C; pasa al tick 0.
    check(GuitarEngineTestAccess::tick(f.engine) == 0,
          "PIMAMI finishes a full cycle without restarting");

    advanceOne(); // Nuevo P: aplica el F pendiente.
    check(GuitarEngineTestAccess::activeChord(f.engine) == 6,
          "new F chord must apply at the next P");
    info = f.engine.chordDebugState();
    check(info[0] == 6 && info[1] == 6 && info[2] == 6 && info[3] == 2,
          "diagnostic confirms actual F applied on second P");
    check(GuitarEngineTestAccess::tick(f.engine) == 1,
          "next cycle advances normally to I");
    advanceOne();
    check(GuitarEngineTestAccess::sampleOnString(f.engine, 3) == 3,
          "new cycle I plays the F voicing sample");
}

static void liveChordChangeKeepsAudioUntilNextNote() {
    Fixture f;
    f.engine.setChord(1); // C, primera cuerda al aire.
    f.engine.play();
    f.render(1);
    f.schedule(0, GuitarEngineTestAccess::frame(f.engine));
    f.render(1);

    f.engine.setChord(6); // F, primera cuerda traste 1.
    const auto oldTail = f.render(1200);
    check(std::all_of(oldTail.begin(), oldTail.end(), [](float s) {
        return near(s, .25f);
    }), "changing chord must not introduce a 20ms dip");

    const auto next = GuitarEngineTestAccess::frame(f.engine) + 100;
    f.schedule(1, next);
    auto lead = f.render(100);
    check(std::all_of(lead.begin(), lead.end(), [](float s) {
        return near(s, .25f);
    }), "old chord sustains right until next note");

    auto attack = f.render(1);
    check(near(attack[0], .375f),
          "new chord sample enters with per-string crossfade");
    f.render(240);
    check(near(f.render(1)[0], .125f),
          "new chord sample continues after old note fades");
}

static void minorSeventhsHaveCorrectPitchesAndLiveSelection() {
    Fixture f;
    check(GuitarEngine::kChordCount == 49, "49 chords including OPEN");
    for (int id = 37; id <= 48; ++id) {
        check(GuitarEngineTestAccess::validMinorSeventh(f.engine, id),
              "minor seventh voicing must contain root, b3, fifth and b7");
    }

    // El preset no contiene acordes: las muestras dependen del botón pulsado.
    int tick=0, offset=0, type=GuitarEngine::EVENT_PLUCK_STRING;
    int target=GuitarEngine::ALL_ALLOWED, layer=GuitarEngine::MF;
    int physical=1, release=20, human=0;
    float velocity=.8f, spread=0;
    check(f.engine.setCustomPattern(16, &tick, &offset, &type, &target,
                                    &layer, &physical, &velocity, &spread,
                                    &release, &human, 1),
          "minor-seventh custom pattern accepted");

    f.engine.setChord(41); // Em7, primera cuerda al aire
    f.engine.play();
    f.render(1);
    check(GuitarEngineTestAccess::activeChord(f.engine) == 41,
          "Em7 selected during playback");
    auto frame = GuitarEngineTestAccess::frame(f.engine) + 100;
    GuitarEngineTestAccess::triggerCustomEvent(f.engine, 0, frame);
    check(GuitarEngineTestAccess::pendingSampleIndices(f.engine) ==
          std::vector<int>({0}), "Em7 uses first-string fret 0 recording");

    const int tickBefore = GuitarEngineTestAccess::tick(f.engine);
    f.engine.setChord(42); // Fm7, primera cuerda al traste 1
    f.render(1);
    check(GuitarEngineTestAccess::activeChord(f.engine) == 42,
          "Fm7 selected without leaving JSON mode");
    check(GuitarEngineTestAccess::tick(f.engine) == tickBefore,
          "changing to Fm7 preserves rhythmic phase");
    check(GuitarEngineTestAccess::pending(f.engine) == 0,
          "pending attacks from Em7 must be cancelled");

    frame = GuitarEngineTestAccess::frame(f.engine) + 100;
    GuitarEngineTestAccess::triggerCustomEvent(f.engine, 0, frame);
    check(GuitarEngineTestAccess::pendingSampleIndices(f.engine) ==
          std::vector<int>({1}), "Fm7 uses first-string fret 1 recording");
}

static void pendingCapacityIsBounded() {
    Fixture f;
    for (int i=0; i<GuitarEngine::kMaxPendingAttacks+20; ++i) f.schedule(0,100+i);
    check(GuitarEngineTestAccess::pending(f.engine)==GuitarEngine::kMaxPendingAttacks,
          "overflow cannot exceed fixed pending storage");
    f.render(1200);
    check(GuitarEngineTestAccess::pending(f.engine)==0, "accepted attacks drain completely");
}

static void concurrentSnapshotsAreCoherent() {
    struct State { uint64_t version=0; std::array<uint64_t,128> values{}; };
    LatestControl<State> controls;
    std::atomic<int> done{0};
    const auto publish = [&] {
        for (int i=0; i<50000; ++i) controls.update([](State& s) {
            ++s.version; s.values.fill(s.version);
        });
        done.fetch_add(1,std::memory_order_release);
    };
    std::thread first(publish);
    std::thread second(publish);
    State state;
    uint64_t seen=0;
    while (done.load(std::memory_order_acquire)<2 || seen<100000) {
        if (!controls.consume(state)) { std::this_thread::yield(); continue; }
        check(state.version>=seen, "snapshot versions never go backwards");
        check(std::all_of(state.values.begin(),state.values.end(),[&](uint64_t v) {
            return v==state.version;
        }), "consumer never sees mixed payload/header during concurrent publication");
        seen=state.version;
    }
    first.join();
    second.join();
}

int main() {
    futureAttackKeepsPreviousNote();
    futureNotesSurviveAndStayChronological();
    simultaneousSameStringUsesLastAttack();
    chordMuteCancelsPendingString();
    chordChangePreservesPlayingPitchWhenStringRemainsAllowed();
    chordChangePreservesCommonPitch();
    customReleaseTargetRespectsItsScheduledFrame();
    controlChangesApplyOnAudioThread();
    customPatternCopiesInputAndLatestState();
    pendingCapacityIsBounded();
    maskedStrumSupportsFourBarTicksAndExactStrings();
    customStrumUsesLatestChordWithoutRestartingRhythm();
    minorSeventhsHaveCorrectPitchesAndLiveSelection();
    pimamiSwitchesChordAtNextPWithoutRestarting();
    liveChordChangeKeepsAudioUntilNextNote();
    concurrentSnapshotsAreCoherent();
    std::cout << "PASS: 16 native regressions (real engine PCM + concurrent control snapshots)\n";
}
