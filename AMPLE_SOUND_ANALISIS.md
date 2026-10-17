# Ample Sound: inspección inicial

Análisis ampliado de ritmos, articulaciones y estructura: [informe detallado](analysis/ample-sound/INFORME_RITMOS_ARTICULACIONES.md).

Comparación posterior con fuente actual de GuitarPilot y 12 funciones nativas recuperadas: [comparación AGML/GuitarPilot](analysis/ample-sound/COMPARACION_AGML_GUITARPILOT.md).

Fecha: 17 de octubre de 2026. Inspección estática; no se ejecutó el instrumento ni se modificó su instalación.

## Identidad y archivos observados

- Carpeta solicitada: `C:\Program Files\Ample Sound`. Contiene `RectanglesVST3.exe` (15.632.384 bytes), `unins000.exe` y `unins000.dat`.
- Metadatos de RectanglesVST3: versión 2.0.0, Ample Sound Co., Ltd.
- El registro de desinstalación referencia el plugin, la biblioteca y sus directorios de datos.
- `C:\Program Files\Common Files\VST3\AGML.vst3\Contents\Resources\moduleinfo.json` declara Ample Guitar M Lite 4.0.0, VST SDK 3.7.8, con clases de instrumento, controlador y compatibilidad.
- Binario del instrumento: `C:\Program Files\Common Files\VST3\AGML.vst3\Contents\x86_64-win\AGML.vst3`, 58.011.648 bytes.

## Datos observados en C:\ProgramData\Ample Sound

| Directorio | Archivos recursivos | Tamaño (MiB) |
|---|---:|---:|
| AGML | 14 | 0,85 |
| AGML_Library | 82 | 877,57 |
| Chord Banks | 37 | 0,21 |
| FX | 166 | 0,44 |
| Manuals | 14 | 36,80 |
| Riff4 | 20 | 0,52 |
| Strum Patterns | 20 | 0,01 |
| Tabs | 4 | 0,07 |

La biblioteca contiene 80 archivos `.ascf` y dos `.cf`. No se ha establecido su codificación ni cómo se relacionan sus contenidos con las articulaciones del instrumento.

El patrón `060-16-44-Pop01.strumseq` es XML: declara categoría Pop, compás 4/4, cuantización 1/16 y BPM 60. Sus eventos contienen Note, Velocity, Begin y End. La función musical de cada valor Note necesita corroboración antes de traducirlo a GuitarPilot.

## Evidencia REA y límites

REA inspeccionó los recursos PE de RectanglesVST3 mediante `inspect_pe_resources`:

- Evidencia: `ev_8790d4b4ab9781ace7116b77c400ee6997ccfd64e487b4aecd9d9f9490aca9a0`.
- SHA-256: `c5b84bb3c1e7f1694fc392839390f292ec3142e3f239aaf344bcd5caba6ee1f2`.
- Formato observado: PE32+, machine 34404 (x86-64).

La consulta de sesión informó que Ghidra no está configurado (`GHIDRA_INSTALL_DIR` ausente). No se obtuvo pseudocódigo ni se verificó el funcionamiento interno del motor de audio. La identificación de archivos, versiones y patrones procede de metadatos y archivos locales; no demuestra comportamiento en ejecución.

## Próximos focos posibles

1. Describir el esquema y los eventos de los patrones de rasgueo y bancos de acordes.
2. Investigar la organización del banco de sonido y las articulaciones documentadas.
3. Preparar análisis nativo del plugin cuando exista un proveedor de descompilación configurado.
