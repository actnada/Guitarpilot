@echo off
setlocal
set VSCMD_SKIP_SENDTELEMETRY=1
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
if not exist build\native-tests mkdir build\native-tests
cl /nologo /std:c++17 /EHsc /W4 /O2 /Iapp\src\main\cpp /Iapp\src\main\cpp\tests\stubs app\src\main\cpp\GuitarEngine.cpp app\src\main\cpp\tests\EngineRegressionTests.cpp /Fobuild\native-tests\ /Febuild\native-tests\EngineRegressionTests.exe
if errorlevel 1 exit /b %errorlevel%
build\native-tests\EngineRegressionTests.exe
exit /b %errorlevel%
