# Ritmo 1 — 4 compases / 45 eventos

## Origen y representación

Patrón suministrado para GuitarPilot basado en grabaciones reales de guitarra, **sin salida MIDI**. Declarado: resolución 480 PPQ, duración 7,680 ticks, 4 compases de 1,920 ticks, cuantización 1/32 y seis cuerdas. Los eventos suministrados se ubican en múltiplos de 120 ticks, por lo que se representan **sin pérdida de posición** en 64 semicorcheas (16 por compás). Por compatibilidad, el campo heredado `ticksPerBar=64` indica aquí la longitud de **todo el ciclo de cuatro compases**.

- 44 rasgueos y un apagado = 45 eventos. Los compases contienen 11, 11, 11 y 12 eventos.
- Propiedad `strings`: selección explícita de cuerdas físicas, 1 (aguda) a 6 (grave); se conservan incluso grupos no contiguos como `[6,4,3,2]`. Los trastes provienen del acorde activo; no se fuerzan notas no declaradas.
- Palm mute conservado en compases 1 y 3, beat 3.75 (ticks 15 y 47). Si no existe muestra de palm mute para la cuerda/traste, el motor conserva su política actual de fallback.
- Apagado del compás 2, beat 3.75 (tick 31) mediante `RELEASE_TARGET` con `releaseMs=20` provisional. `StrumTime=16` del origen no establece un fade en milisegundos.
- Intensidades originales 30, 60, 70, 80, 90 normalizadas a `valor/127`, sin mensajes MIDI.
- `spreadMs=15` es un supuesto experimental al trasladar `StrumTime=15`: no se ha verificado que el campo original tenga unidades de milisegundos.
- El origen no da género ni BPM. Nombre provisional: `Ritmo 1`; `defaultBpm=90` como parámetro de ensayo editable.
- Humanización desactivada inicialmente (`humanize=false`) para comprobar el timing original sin variaciones.
- La propiedad `strings` es opcional y no modifica los ritmos ya existentes. Kotlin la convierte en máscara de 6 bits, enviada por el campo JNI existente `stringNumbers` cuando `target=STRING_MASK (5)`; el C++ limita las muestras según la máscara y el acorde. No se añaden parámetros JNI.

## Referencia del JSON
```json
{
  "tick": 2,
  "type": "STRUM_DOWN",
  "strings": [4, 3, 2],
  "velocity": 0.236,
  "spreadMs": 15,
  "layer": "MF"
}
```

## Tabla musical aportada
### Compás 1

| Beat | Tick | Gesto | Intensidad | Cuerdas | Articulación |
|---:|---:|:---:|---:|---|---|
| 0.00 | 0 | ↓ | 80 | 6, 5, 4, 3 | Sustain |
| 0.50 | 2 | ↓ | 30 | 4, 3, 2 | Sustain |
| 0.75 | 3 | ↑ | 60 | 3, 2, 1 | Sustain |
| 1.00 | 4 | ↓ | 90 | 5, 4, 3, 2 | Sustain |
| 1.75 | 7 | ↑ | 70 | 4, 3, 2, 1 | Sustain |
| 2.25 | 9 | ↑ | 30 | 4, 3, 2 | Sustain |
| 2.50 | 10 | ↓ | 80 | 5, 4, 3, 2 | Sustain |
| 3.00 | 12 | ↓ | 30 | 4, 3, 2 | Sustain |
| 3.25 | 13 | ↑ | 80 | 4, 3, 2, 1 | Sustain |
| 3.50 | 14 | ↓ | 60 | 4, 3, 2 | Sustain |
| 3.75 | 15 | ↑ | 60 | 3, 2, 1 | Palm Mute |

### Compás 2

| Beat | Tick | Gesto | Intensidad | Cuerdas | Articulación |
|---:|---:|:---:|---:|---|---|
| 0.00 | 16 | ↓ | 80 | 6, 5, 4, 3, 2 | Sustain |
| 0.50 | 18 | ↓ | 30 | 6, 4, 3, 2 | Sustain |
| 0.75 | 19 | ↑ | 30 | 6, 4, 3, 2 | Sustain |
| 1.00 | 20 | ↓ | 90 | 6, 5, 4, 3, 2 | Sustain |
| 1.75 | 23 | ↑ | 60 | 6, 3, 2, 1 | Sustain |
| 2.25 | 25 | ↑ | 60 | 6, 3, 2, 1 | Sustain |
| 2.50 | 26 | ↓ | 80 | 6, 5, 4, 3, 2 | Sustain |
| 3.00 | 28 | ↓ | 30 | 6, 4, 3, 2 | Sustain |
| 3.25 | 29 | ↑ | 70 | 6, 4, 3, 2, 1 | Sustain |
| 3.50 | 30 | ↓ | 60 | 6, 4, 3, 2 | Sustain |
| 3.75 | 31 | × | 60 | — | Apagado |

### Compás 3

| Beat | Tick | Gesto | Intensidad | Cuerdas | Articulación |
|---:|---:|:---:|---:|---|---|
| 0.00 | 32 | ↓ | 80 | 6, 5, 4, 3 | Sustain |
| 0.50 | 34 | ↓ | 30 | 4, 3, 2 | Sustain |
| 0.75 | 35 | ↑ | 60 | 3, 2, 1 | Sustain |
| 1.00 | 36 | ↓ | 90 | 4, 3, 2, 1 | Sustain |
| 1.75 | 39 | ↑ | 70 | 4, 3, 2, 1 | Sustain |
| 2.25 | 41 | ↑ | 30 | 4, 3, 2 | Sustain |
| 2.50 | 42 | ↓ | 80 | 4, 3, 2 | Sustain |
| 3.00 | 44 | ↓ | 30 | 4, 3, 2 | Sustain |
| 3.25 | 45 | ↑ | 80 | 4, 3, 2, 1 | Sustain |
| 3.50 | 46 | ↓ | 60 | 4, 3, 2 | Sustain |
| 3.75 | 47 | ↑ | 60 | 3, 2, 1 | Palm Mute |

### Compás 4

| Beat | Tick | Gesto | Intensidad | Cuerdas | Articulación |
|---:|---:|:---:|---:|---|---|
| 0.00 | 48 | ↓ | 80 | 6, 5, 4, 3, 2, 1 | Sustain |
| 0.50 | 50 | ↓ | 30 | 6, 5, 4, 3, 2 | Sustain |
| 0.75 | 51 | ↑ | 30 | 6, 5, 3, 2, 1 | Sustain |
| 1.00 | 52 | ↓ | 90 | 6, 5, 4, 3, 2, 1 | Sustain |
| 1.50 | 54 | ↑ | 30 | 6, 5, 3, 2, 1 | Sustain |
| 1.75 | 55 | ↑ | 80 | 6, 5, 3, 2, 1 | Sustain |
| 2.25 | 57 | ↑ | 30 | 6, 5, 3, 2, 1 | Sustain |
| 2.50 | 58 | ↓ | 90 | 6, 5, 4, 3, 2, 1 | Sustain |
| 3.00 | 60 | ↓ | 30 | 6, 5, 4, 3, 2 | Sustain |
| 3.25 | 61 | ↑ | 80 | 6, 5, 4, 3, 2, 1 | Sustain |
| 3.50 | 62 | ↓ | 60 | 6, 5, 4, 3, 2 | Sustain |
| 3.75 | 63 | ↑ | 60 | 6, 5, 3, 2, 1 | Sustain |

## Validación pendiente en dispositivo
1. Ejecutar `python scripts/validar_ritmos.py` desde la raíz y los tests nativos de host.
2. Compilar con Gradle en Android; comprobar que aparece `Ritmo 1` en el selector.
3. Probar con acordes que permitan todas las cuerdas y con acordes como C y D que mutean graves; evitar que aparezcan cuerdas no permitidas.
4. Escuchar el apagado en compás 2, los palm mutes y los grupos parciales de cuerdas. Verificar ausencia de clics y saturación antes de ajustar humanización y velocidad.

**No se han ejecutado aún las pruebas de escucha en un dispositivo real.**
