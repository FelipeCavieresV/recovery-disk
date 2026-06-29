@echo off
echo === Compilando DemogoRecovery en Release ===
cd /d "%~dp0build"

cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
if %ERRORLEVEL% NEQ 0 (
    echo === ERROR en configuracion ===
    pause
    exit /b 1
)

cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo === ERROR EN COMPILACION ===
    pause
    exit /b 1
)

echo.
echo === Copiando DLLs con windeployqt ===
set WINDEPLOYQT=C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe
set EXE="%~dp0build\Release\DemogoRecovery.exe"

if exist %WINDEPLOYQT% (
    %WINDEPLOYQT% --release %EXE%
    echo DLLs copiadas correctamente.
) else (
    echo AVISO: windeployqt no encontrado en %WINDEPLOYQT%
)

echo.
echo === COMPILACION EXITOSA ===
echo Ejecuta: build\Release\DemogoRecovery.exe
pause
