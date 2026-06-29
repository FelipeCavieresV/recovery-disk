@echo off
echo Copiando DLLs de Release a Debug...

set SRC="%~dp0build\Release"
set DST="%~dp0build\Debug"

xcopy /E /Y /I %SRC%\*.dll %DST%\
xcopy /E /Y /I %SRC%\generic %DST%\generic\
xcopy /E /Y /I %SRC%\iconengines %DST%\iconengines\
xcopy /E /Y /I %SRC%\imageformats %DST%\imageformats\
xcopy /E /Y /I %SRC%\multimedia %DST%\multimedia\
xcopy /E /Y /I %SRC%\networkinformation %DST%\networkinformation\
xcopy /E /Y /I %SRC%\platforms %DST%\platforms\
xcopy /E /Y /I %SRC%\styles %DST%\styles\
xcopy /E /Y /I %SRC%\tls %DST%\tls\

echo.
echo Listo. Ahora ejecuta build\Debug\DemogoRecovery.exe como administrador.
pause
