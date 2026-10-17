# Acordes menores séptima (m7) — GuitarPilot version-b

GuitarPilot es un instrumento de **acompañamiento en vivo**: el usuario decide los cambios de acorde pulsando uno de seis botones configurables, sin detener el ritmo JSON ni seleccionar una canción pregrabada.

## Catálogo extendido

- 0: OPEN.
- 1–12: mayores C..B.
- 13–24: menores Cm..Bm.
- 25–36: séptimas dominantes C7..B7 (las existentes; no son maj7).
- **37–48: séptimas menores Cm7..Bm7**.

En total: **49 entradas (48 acordes + OPEN)**. Se mantienen los IDs 0..36, las asignaciones por defecto de los seis botones (C, G, D, Em, Am, Bm), la reasignación con pulsación larga y el cambio de acordes en vivo sin reiniciar el patrón.

### Digitaciones

Orden: cuerdas **6,5,4,3,2,1**, `x` = cuerda silenciada, `0` = al aire.

| ID | Acorde | Digitación | Bajo raíz | Bajo alternativo |
|---:|---|---|---|---|
| 37 | Cm7 | x-3-5-3-4-3 | 5ª traste 3 | 4ª traste 5 |
| 38 | C#m7 | x-4-6-4-5-4 | 5ª traste 4 | 4ª traste 6 |
| 39 | Dm7 | x-x-0-2-1-1 | 4ª al aire | 3ª traste 2 |
| 40 | D#m7 | x-6-8-6-7-6 | 5ª traste 6 | 4ª traste 8 |
| 41 | Em7 | 0-2-2-0-3-0 | 6ª al aire | 5ª traste 2 |
| 42 | Fm7 | 1-3-1-1-1-1 | 6ª traste 1 | 5ª traste 3 |
| 43 | F#m7 | 2-4-2-2-2-2 | 6ª traste 2 | 5ª traste 4 |
| 44 | Gm7 | 3-5-3-3-3-3 | 6ª traste 3 | 5ª traste 5 |
| 45 | G#m7 | 4-6-4-4-4-4 | 6ª traste 4 | 5ª traste 6 |
| 46 | Am7 | x-0-2-0-1-0 | 5ª al aire | 4ª traste 2 |
| 47 | A#m7 | x-1-3-1-2-1 | 5ª traste 1 | 4ª traste 3 |
| 48 | Bm7 | x-2-4-2-3-2 | 5ª traste 2 | 4ª traste 4 |

Cada digitación incorpora exactamente las clases de nota **1, ♭3, 5 y ♭7**. Las cuerdas mudas (`-1`) siguen excluidas de rasgueos automáticos y patrones con máscara de cuerdas.

Los acordes `m7` **no** son mayores séptima (`maj7`): por ejemplo, `Cm7 = C, Eb, G, Bb`.

## Pruebas

La prueba `minorSeventhsHaveCorrectPitchesAndLiveSelection` comprueba todos los intervalos de los 12 acordes, cuerdas muteadas, correspondencia con el bajo raíz y transición `Em7 → Fm7` sin reiniciar el ritmo. El selector tanto en `MainActivity` como en `RhythmEditorActivity` lista los nuevos acordes en una categoría independiente.

Ejecuta las regresiones del motor en un entorno compatible con el host, compila la app e instala en Android mediante `gradlew.bat :app:installDebug`. Valida a oído especialmente los acordes sostenidos y los cambios rápidos. Esta actualización aún no se ha comprobado en un teléfono.
