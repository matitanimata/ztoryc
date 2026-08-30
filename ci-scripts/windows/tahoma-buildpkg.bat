@echo off

cd toonz\build

echo ">>> Creating Ztoryc directory"

IF EXIST Ztoryc rmdir /S /Q Ztoryc

mkdir Ztoryc

echo ">>> Copy Ztoryc DLLs"

copy /y RelWithDebInfo\*.* Ztoryc

echo ">>> Copy ThirdParty DLLs"
copy /Y ..\..\thirdparty\freeglut\bin\x64\freeglut.dll Ztoryc
copy /Y ..\..\thirdparty\glew\glew-1.9.0\bin\64bit\glew32.dll Ztoryc
copy /Y ..\..\thirdparty\libmypaint\dist\64\libiconv-2.dll Ztoryc
copy /Y ..\..\thirdparty\libmypaint\dist\64\libintl-8.dll Ztoryc
copy /Y ..\..\thirdparty\libmypaint\dist\64\libjson-c-2.dll Ztoryc
copy /Y ..\..\thirdparty\libmypaint\dist\64\libmypaint-1-4-0.dll Ztoryc

echo ">>> Copy OpenCV DLLs"
IF EXIST C:\tools\opencv (
   copy /Y "C:\tools\opencv\build\x64\vc16\bin\opencv_world4110.dll" Ztoryc
) ELSE (
   copy /Y "C:\opencv\4110\build\x64\vc16\bin\opencv_world4110.dll" Ztoryc
)

IF EXIST ..\..\thirdparty\canon\Header (
   echo ">>> Copy Canon EDSDK DLLs"
   copy /Y ..\..\thirdparty\canon\Dll\EDSDK.dll Ztoryc
   copy /Y ..\..\thirdparty\canon\Dll\EdsImage.dll Ztoryc
)

IF EXIST ..\..\thirdparty\libgphoto2\include (
   echo ">>> Copy Libgphoto2 DLLs"
   xcopy /Y /E ..\..\thirdparty\libgphoto2\bin Ztoryc
)

echo ">>> Copy MSVC DLLs"
set VCINSTALLDIR="C:\Program Files\Microsoft Visual Studio\2022\Community\VC"
IF EXIST "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC" set VCINSTALLDIR="C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC"
echo "VCINSTALLDIR=%VCINSTALLDIR%"

set VCRUNTIME_PATH=
for /d /r ""%VCINSTALLDIR%"" %%a in (14.*) do (
    if exist "%%a\x64" (
        for /d %%b in ("%%a\x64\*") do (
            if exist "%%b\vcruntime*.dll" (
                set "VCRUNTIME_PATH=%%b"
                goto :done
            )
        )
    )
)
:done
echo "VCRUNTIME_PATH=%VCRUNTIME_PATH%"

copy /Y "%VCRUNTIME_PATH%\vcruntime140.dll" Ztoryc
copy /Y "%VCRUNTIME_PATH%\vcruntime140_1.dll" Ztoryc
copy /Y "%VCRUNTIME_PATH%\msvcp140.dll" Ztoryc
copy /Y "%VCRUNTIME_PATH%\msvcp140_1.dll" Ztoryc
copy /Y "%VCRUNTIME_PATH%\msvcp140_2.dll" Ztoryc

echo ">>> Configuring Ztoryc.exe for deployment"

REM Setup for local builds
set QT_PATH=C:\Qt\5.15.2_wintab\msvc2019_64

REM These are effective when running from Actions/Appveyor
IF EXIST ..\..\thirdparty\qt\5.15.2_wintab\msvc2019_64 set QT_PATH=..\..\thirdparty\qt\5.15.2_wintab\msvc2019_64
echo "QT_PATH=%QT_PATH%"


%QT_PATH%\bin\windeployqt.exe Ztoryc\Ztoryc.exe --opengl

REM Qt fa HTTPS solo se trova OpenSSL, e windeployqt NON lo copia: le librerie
REM TLS non fanno parte di Qt. Senza queste due DLL ogni connessione cifrata
REM muore con "TLS initialization failed" — Kitsu compreso. Qt 5.15.2 vuole
REM la serie 1.1.1, con questi nomi esatti; la 3.x ha altra ABI e non va.
echo ">>> Copy OpenSSL DLLs (TLS support for Qt)"
copy /Y ..\..\thirdparty\openssl\bin\x64\libssl-1_1-x64.dll Ztoryc
copy /Y ..\..\thirdparty\openssl\bin\x64\libcrypto-1_1-x64.dll Ztoryc
if not exist Ztoryc\libssl-1_1-x64.dll (
   echo ERROR: OpenSSL DLLs missing — the build would ship without HTTPS.
   exit /b 1
)


IF EXIST ..\..\thirdparty\apps\ffmpeg\bin (
   echo ">>> Copying FFmpeg to Ztoryc\ffmpeg"
   IF EXIST Ztoryc\ffmpeg rmdir /S /Q Ztoryc\ffmpeg
   xcopy /Y /E /I ..\..\thirdparty\apps\ffmpeg\bin Ztoryc\ffmpeg
)

IF EXIST ..\..\thirdparty\apps\rhubarb (
   echo ">>> Copying Rhubarb Lip Sync to Ztoryc\rhubarb"
   IF EXIST Ztoryc\rhubarb rmdir /S /Q Ztoryc\rhubarb
   mkdir Ztoryc\rhubarb
   copy /Y ..\..\thirdparty\apps\rhubarb\rhubarb.exe Ztoryc\rhubarb
   xcopy /Y /E /I ..\..\thirdparty\apps\rhubarb\res "Ztoryc\rhubarb\res"
)

REM ── Lip sync: Vosk + Whisper + espeak-ng ────────────────────────────────────
REM I pezzi li mettono ci-scripts\fetch-lipsync-deps.sh e build-lipsync-tools.sh
REM (girano in Git Bash anche qui: uno script solo per i tre sistemi).
REM
REM ATTENZIONE: se mancano, la build RIESCE e l'applicazione parte — il lip sync
REM si ripiega in silenzio. Per questo sotto c'e' un controllo esplicito che
REM ferma tutto, come per gli helper LZO.
IF EXIST ..\..\thirdparty\apps\lipsync (
   echo ">>> Copying Vosk to Ztoryc\vosk"
   IF EXIST Ztoryc\vosk rmdir /S /Q Ztoryc\vosk
   mkdir Ztoryc\vosk
   copy /Y ..\..\thirdparty\apps\lipsync\libvosk.dll Ztoryc\vosk
   copy /Y ..\..\thirdparty\apps\lipsync\*.zvosk Ztoryc\vosk

   echo ">>> Copying Whisper to Ztoryc\whisper"
   IF EXIST Ztoryc\whisper rmdir /S /Q Ztoryc\whisper
   mkdir Ztoryc\whisper
   copy /Y ..\..\thirdparty\apps\lipsync\ggml-*.bin Ztoryc\whisper
   IF EXIST ..\..\thirdparty\apps\lipsync\whisper-cli.exe (
      copy /Y ..\..\thirdparty\apps\lipsync\whisper-cli.exe Ztoryc\whisper
   )

   IF EXIST ..\..\thirdparty\apps\lipsync\espeak-ng.exe (
      echo ">>> Copying espeak-ng to Ztoryc\espeak"
      IF EXIST Ztoryc\espeak rmdir /S /Q Ztoryc\espeak
      mkdir Ztoryc\espeak
      copy /Y ..\..\thirdparty\apps\lipsync\espeak-ng.exe Ztoryc\espeak
      xcopy /Y /E /I ..\..\thirdparty\apps\lipsync\espeak-ng-data "Ztoryc\espeak\espeak-ng-data"
   )

   REM Il controllo che rende inutile fidarsi.
   FOR %%F IN (
      "Ztoryc\vosk\libvosk.dll"
      "Ztoryc\vosk\en.zvosk"
      "Ztoryc\vosk\it.zvosk"
      "Ztoryc\whisper\ggml-base-q5_1.bin"
      "Ztoryc\whisper\whisper-cli.exe"
      "Ztoryc\espeak\espeak-ng.exe"
      "Ztoryc\espeak\espeak-ng-data\phontab"
   ) DO (
      IF NOT EXIST %%F (
         echo ERRORE: manca nel pacchetto %%F
         exit /b 1
      )
   )
   echo ">>> Lip sync bundled"
) ELSE (
   echo WARNING: thirdparty\apps\lipsync assente, il pacchetto NON avra' il lip sync
)

echo ">>> Remove unnecessary files"
REM Remove github keep files
del /A- /S ..\..\stuff\*.gitkeep

echo ">>> Creating Ztoryc Windows Installer"
IF NOT EXIST installer mkdir installer
cd installer

IF EXIST program rmdir /S /Q program
xcopy /Y /E /I ..\Ztoryc program

IF EXIST stuff rmdir /S /Q stuff
xcopy /Y /E /I ..\..\..\stuff stuff

python ..\..\installer\windows\filelist_python3.py %cd%

REM Read the Ztoryc semver from the single source of truth and pass it to the
REM Inno Setup compiler so the installer version matches the app — otherwise
REM setup.iss falls back to its hardcoded default (the old Tahoma2D 1.6).
set ZVER_FILE=..\..\cmake\ZtorycVersion.cmake
for /f "tokens=3 delims=() " %%a in ('findstr /C:"ZTORYC_VERSION_MAJOR " "%ZVER_FILE%"') do set ZVMAJ=%%a
for /f "tokens=3 delims=() " %%a in ('findstr /C:"ZTORYC_VERSION_MINOR " "%ZVER_FILE%"') do set ZVMIN=%%a
for /f "tokens=3 delims=() " %%a in ('findstr /C:"ZTORYC_VERSION_PATCH " "%ZVER_FILE%"') do set ZVPAT=%%a
set ZTORYC_VERSION=%ZVMAJ%.%ZVMIN%.%ZVPAT%
echo "Installer version: %ZTORYC_VERSION%"

ISCC.exe /DMyAppVersion=%ZTORYC_VERSION% /I. /O.. ..\..\installer\windows\setup.iss

cd ..

echo ">>> Creating Ztoryc Windows Portable package"

xcopy /Y /E /I ..\..\stuff Ztoryc\ztorycstuff

IF EXIST Ztoryc-portable-win.zip del Ztoryc-portable-win.zip
7z a Ztoryc-portable-win.zip Ztoryc


cd ../..
