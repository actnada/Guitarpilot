#!/usr/bin/env python3
"""Validación ligera de presets JSON de GuitarPilot version-b, solo stdlib."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "app/src/main/assets/rhythms"
REGISTRY = ROOT / "app/src/main/java/com/example/guitarpilot/RhythmPresets.kt"
TYPES = {"STRUM_DOWN", "STRUM_UP", "ROOT_BASS", "ALT_BASS", "PLUCK_STRING", "RELEASE_TARGET"}
TARGETS = {"ALL_ALLOWED", "BASS_3", "TREBLE_3", "TREBLE_4", "ROOT_BASS"}
COUNTS = {"ample_pop_02.json":14,"ample_pop_03.json":16,"ample_pop_04.json":11,"ample_pop_05.json":13,"ample_pop_06.json":13,"ample_folk_02.json":9,"ample_folk_03.json":16,"ample_folk_04.json":16,"ample_folk_05.json":16,"ritmo_01.json":45}
EXPECTED_NAMES = {"Balada 1", "Pop 1", "Rock", "Folk 1", "Balada 2", "Pop 2", "Pop 3", "Pop 4", "Pop 5", "Pop 6", "Folk 2", "Folk 3", "Folk 4", "Folk 5", "Ritmo 1"}

def main():
    catalog = REGISTRY.read_text(encoding="utf-8")
    matches = re.findall(r'RhythmPreset\(\d+,\s*"[^"]+",\s*"rhythms/([^"]+\.json)"\)', catalog)
    found = sorted(f.name for f in ASSETS.glob("*.json"))
    assert len(matches) == len(set(matches)) == 15, "Catálogo incompleto o duplicado"
    assert set(matches) == set(found), "Archivos y catálogo no coinciden"
    names, ids = set(), set()
    total = 0
    for name in found:
        d = json.loads((ASSETS / name).read_text(encoding="utf-8"))
        assert d["schemaVersion"] == 1 and 40 <= d["defaultBpm"] <= 220, name
        assert 1 <= d["ticksPerBar"] <= 64 and 1 <= len(d["events"]) <= 128, name
        assert d["name"] not in names and d["id"] not in ids, name
        names.add(d["name"]); ids.add(d["id"])
        if name in COUNTS:
            assert len(d["events"]) == COUNTS[name], name
        for e in d["events"]:
            assert 0 <= e["tick"] < d["ticksPerBar"], name
            assert e["type"] in TYPES and e.get("target", "ALL_ALLOWED") in TARGETS, name
            assert e.get("layer", "MF") in {"MF", "PALM_MUTE"}, name
            assert -100 <= e.get("offsetMs", 0) <= 100, name
            assert 0 <= e.get("velocity", 0.8) <= 1, name
            assert 0 <= e.get("spreadMs", 13) <= 60, name
            assert 1 <= e.get("releaseMs", 20) <= 250, name
            assert 1 <= e.get("stringNumber", 1) <= 6, name
            if "strings" in e:
                chosen = e["strings"]
                assert e["type"] in {"STRUM_DOWN", "STRUM_UP"}, name
                assert isinstance(chosen, list) and 1 <= len(chosen) <= 6, name
                assert all(isinstance(v, int) and not isinstance(v, bool) and
                           1 <= v <= 6 for v in chosen), name
                assert len(set(chosen)) == len(chosen), name
        total += len(d["events"])
    assert names == EXPECTED_NAMES, "Nombres de presets inesperados"
    assert len(COUNTS) == 10 and sum(COUNTS.values()) == 169
    added = json.loads((ASSETS / "ritmo_01.json").read_text(encoding="utf-8"))
    assert added["ticksPerBar"] == 64 and len(added["events"]) == 45
    assert [sum(16*b <= e["tick"] < 16*(b+1) for e in added["events"])
            for b in range(4)] == [11, 11, 11, 12]
    assert sum(e["type"] == "RELEASE_TARGET" for e in added["events"]) == 1
    assert sum(e.get("layer") == "PALM_MUTE" for e in added["events"]) == 2
    assert sum("strings" in e for e in added["events"]) == 44
    print(f"OK: {len(found)} ritmos, {total} eventos (10 importados, 169 eventos)")

if __name__ == "__main__":
    main()
