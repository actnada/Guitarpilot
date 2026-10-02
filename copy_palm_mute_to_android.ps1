$ErrorActionPreference = "Stop"

$srcRoot = "C:\Users\admin\Downloads\GuitarPilot_PalmMute_MF"
$dst = "C:\Users\admin\AndroidStudioProjects\GuitarPilotOboe\app\src\main\res\raw"

$slots = @(
    @{s=6; f=0; note="e2"},
    @{s=6; f=3; note="g2"},

    @{s=5; f=0; note="a2"},
    @{s=5; f=2; note="b2"},
    @{s=5; f=3; note="c3"},

    @{s=4; f=0; note="d3"},
    @{s=4; f=2; note="e3"},
    @{s=4; f=4; note="fs3"},

    @{s=3; f=0; note="g3"},
    @{s=3; f=2; note="a3"},
    @{s=3; f=4; note="b3"},

    @{s=2; f=0; note="b3"},
    @{s=2; f=1; note="c4"},
    @{s=2; f=3; note="d4"},

    @{s=1; f=0; note="e4"},
    @{s=1; f=2; note="fs4"},
    @{s=1; f=3; note="g4"}
)

New-Item -ItemType Directory -Force -Path $dst | Out-Null

foreach ($x in $slots) {
    $fret = "{0:D2}" -f $x.f

    $pattern = "s$($x.s)_f${fret}_$($x.note)_palmmute_mf_MicRos_20230202_take1.wav"

    $found = Get-ChildItem $srcRoot -Recurse -File -Filter $pattern | Select-Object -First 1

    if (-not $found) {
        Write-Host "FALTA: $pattern" -ForegroundColor Red
        continue
    }

    $newName = "pm_s$($x.s)_f${fret}_$($x.note)_mf.wav"
    Copy-Item $found.FullName (Join-Path $dst $newName) -Force

    Write-Host "OK: $newName" -ForegroundColor Green
}

Write-Host ""
Write-Host "Palm Mute copiados:"
(Get-ChildItem $dst -Filter "pm_*.wav").Count
