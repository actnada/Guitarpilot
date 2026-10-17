# Ritmos de guitarra reales · rama version-b

## Estado
Se agregaron 9 **prototipos experimentales** a partir de las 9 tablas completas de `gemini-code-1792252736024.md` (124 golpes y 20 silencios en 144 pasos). El décimo encabezado no contiene pasos. Los nuevos patrones reproducen **samples reales** del motor nativo, no generan sonidos MIDI. Los patrones preexistentes conservan su secuencia; solo se simplificaron sus nombres visibles.

| Ritmo | Procedencia | BPM | Golpes |
|---|---|---:|---:|
| Pop 2 | `060-16-44-Pop01.strumseq` | 60 | 14 |
| Pop 3 | `060-16-44-Pop02a.strumseq` | 60 | 16 |
| Pop 4 | `080-16-44-POP-03.strumseq` | 80 | 11 |
| Pop 5 | `080-16-44-Pop01a.strumseq` | 80 | 13 |
| Pop 6 | `100-16-44-POP-01.strumseq` | 100 | 13 |
| Folk 2 | `060-16-44Tri-Folk-03.strumseq` | 60 | 9 |
| Folk 3 | `070-16-44-Folk-01.strumseq` | 70 | 16 |
| Folk 4 | `070-16-44-Folk-04.strumseq` | 70 | 16 |
| Folk 5 | `070-16-44-Folk-05.strumseq` | 80 | 16 |

## Conversiones y límites
- Se preservan posiciones de golpe/silencio, BPM declarado en la descripción y niveles relativos de intensidad.
- Los códigos **Nota/Gesto** del origen NO tienen una equivalencia verificada con DOWN/UP/bajo/cuerdas. La dirección actual es una **hipótesis de prueba** (ticks pares = DOWN, impares = UP), no una extracción de Ample Sound.
- Pulsos `tick % 4 = 0`: todas las cuerdas permitidas; contratiempo `tick % 4 = 2`: 4 agudas; otros: 3 agudas. Esta reducción de densidad es un ajuste para samples grabados, no un reflejo exacto del original.
- Velocidad: `clamp(0.20 + 0.74 * (v/127)^0.85, 0.24, 0.94)`; `spreadMs` = 34/24/16 ms, `humanize=true`. Los eventos respetan los límites del `RhythmJsonCodec` actual.
- **Folk 2**: fuente descrita como `4/4Tri / Shuffle`; el prototipo atrasa 70 ms los ataques en ticks 2, 6, 10 y 14 que existan. No equivale a un shuffle de tresillos preciso.
- **Folk 2**, paso 10: la tabla indica velocidad `65/75`, gesto `71/69`; se utiliza un solo golpe con velocidad promediada (70). No suponemos eventos múltiples.
- **Folk 5**: su archivo original empieza por `070-`, pero la descripción indica 80 BPM. Se respeta la descripción (80 BPM), pendiente de verificación.

## Validación y escucha
Ejecutar `python3 scripts/validar_ritmos.py` en la raíz del repositorio. Después, compilar en Android y probar los 14 presets desde el menú. Comparar los importados con `PIMAMI` y `Pop 1` y escuchar con diferentes acordes, tempos y ganancias. Si hay saturación, sonidos entrecortados o ataques demasiado densos, ajustar direcciones, `target`, `spreadMs`, `velocity` y apagados con el editor de la versión B.

**No se ha compilado Android ni escuchado la ejecución real en un dispositivo.** Los presets son punto de partida musical, no una transcripción exacta de las articulaciones de Ample Sound.

## Valores originales por paso (1 a 16, guion = silencio)
### Pop 2 — `060-16-44-Pop01.strumseq`
- Velocidades: `90 -- 60 -- 30 60 30 100 40 30 70 60 30 100 50 50`
- Nota/Gesto: `72 -- 74 -- 72 76 74 76 78 76 74 76 79 76 81 83`

### Pop 3 — `060-16-44-Pop02a.strumseq`
- Velocidades: `90 30 20 90 15 30 50 70 100 30 20 100 15 30 60 60`
- Nota/Gesto: `72 74 74 72 74 74 74 76 72 74 74 72 74 74 74 76`

### Pop 4 — `080-16-44-POP-03.strumseq`
- Velocidades: `100 -- 70 -- 110 -- 70 50 90 90 90 -- 110 -- 90 40`
- Nota/Gesto: `72 -- 74 -- 72 -- 74 76 77 76 74 -- 72 -- 74 76`

### Pop 5 — `080-16-44-Pop01a.strumseq`
- Velocidades: `100 -- 30 90 -- 30 60 50 80 30 40 100 -- 40 50 61`
- Nota/Gesto: `72 -- 74 76 -- 76 78 78 72 76 74 76 -- 74 78 76`

### Pop 6 — `100-16-44-POP-01.strumseq`
- Velocidades: `110 -- 90 30 70 60 100 -- -- 50 80 80 110 80 90 30`
- Nota/Gesto: `72 -- 74 76 74 77 72 -- -- 76 74 74 72 76 74 78`

### Folk 2 — `060-16-44Tri-Folk-03.strumseq`
- Velocidades: `70 -- 70 60 -- -- 70 -- 50 65/75 -- 50 90 -- 90 --`
- Nota/Gesto: `72 -- 69 74 -- -- 72 -- 74 71/69 -- 76 74 -- 76 --`

### Folk 3 — `070-16-44-Folk-01.strumseq`
- Velocidades: `85 85 85 85 80 80 80 80 80 40 80 80 90 50 60 80`
- Nota/Gesto: `74 74 74 74 72 72 72 76 76 74 76 76 74 76 74 76`

### Folk 4 — `070-16-44-Folk-04.strumseq`
- Velocidades: `90 90 50 50 105 105 65 40 20 15 40 50 120 50 110 80`
- Nota/Gesto: `72 72 74 74 72 72 74 76 74 76 74 76 74 76 74 76`

### Folk 5 — `070-16-44-Folk-05.strumseq`
- Velocidades: `115 115 115 115 90 90 90 40 20 15 110 110 80 50 80 60`
- Nota/Gesto: `72 72 72 72 72 72 72 76 74 76 74 74 74 76 74 76`

