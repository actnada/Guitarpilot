# Actualización Android de GuitarPilot

## Identidad y versión

- Paquete (Android `applicationId`): `com.example.guitarpilot2`
- Namespace de clases: `com.example.guitarpilot` (NO es el package a instalar/desinstalar).
- Versión de esta entrega: `versionCode=2`, `versionName=1.1`.
- El nombre visible GuitarPilot no determina si Android considera dos APK como la misma aplicación.
- Cada APK de actualización distribuido debe tener `versionCode` mayor que las versiones anteriores de esa instalación y **la misma firma digital**. Cambiar solamente `versionName` no basta.
- `adb install -r` reemplaza una instalación compatible y normalmente conserva los datos; Android puede aceptar también el mismo `versionCode` en ciertas situaciones, pero incrementarlo permite identificar las actualizaciones y evita bloqueos por downgrade.
- APK debug: se firma con la clave de depuración instalada en cada máquina. Si se distribuyen desde computadoras diferentes con claves de depuración distintas, Android no puede actualizarlos entre sí sin desinstalar. Para distribución estable se requiere una clave de firma de publicación persistente y custodiada fuera del repositorio. **Nunca subir claves privadas ni contraseñas a GitHub.**

## Compilar desde Windows y actualizar sin desinstalar

```powershell
cd C:\Users\ACER\GuitarPilotRepo
git switch version-b
git pull --ff-only origin version-b
.\gradlew.bat clean :app:assembleDebug
$adb = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe"
& $adb devices
& $adb install -r ".\app\build\outputs\apk\debug\app-debug.apk"
& $adb shell am start -n com.example.guitarpilot2/com.example.guitarpilot.MainActivity
```

Para la actualización sobre USB o depuración inalámbrica se puede utilizar directamente `.\gradlew.bat :app:installDebug` tras sincronizar el repositorio.

## Diagnóstico sin borrar datos

Comprobar paquete y versión realmente instalados:

```powershell
& $adb shell pm list packages | Select-String "guitarpilot"
& $adb shell dumpsys package com.example.guitarpilot2 | Select-String "versionCode|versionName"
```

Consultar el APK local, si existe `aapt.exe` de Android Build Tools:

```powershell
$aapt = Get-ChildItem "$env:LOCALAPPDATA\Android\Sdk\build-tools\*\aapt.exe" | Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
& $aapt dump badging ".\app\build\outputs\apk\debug\app-debug.apk" | Select-String "package:"
```

Si `adb install -r` devuelve `INSTALL_FAILED_UPDATE_INCOMPATIBLE`, comprobar que la firma del APK nuevo coincida con la instalada. No existe una actualización in-place compatible si las claves son diferentes: requiere firmar con la clave anterior o, solo con autorización expresa y previa copia de datos, desinstalar e instalar desde cero.

Si devuelve `INSTALL_FAILED_VERSION_DOWNGRADE`, el APK es una versión menor; obtener un APK con `versionCode` superior. Si devuelve `INSTALL_FAILED_ALREADY_EXISTS` sin `-r`, usar `-r`.

**No emplear `adb uninstall`, `pm clear` ni `git reset --hard` como primeros pasos.**

## Política para siguientes entregas

Antes de distribuir un APK nuevo:
1. Incrementar `versionCode` (3, 4, 5, ...).
2. Actualizar `versionName` con un nombre legible.
3. Conservar `applicationId` y la clave de firma.
4. Compilar, verificar `aapt dump badging` e instalar sobre la versión anterior.
