@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
if not exist logs mkdir logs

echo Compiling VisionGlove by Grok (MSYS2 ucrt64 g++)...
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /c/Users/nuraa/VisionGlove_by_Grok; g++ -std=c++20 -O2 -Wall -Wextra -I include -o build/visionglove.exe src/logger.cpp src/config.cpp src/security.cpp src/sensors.cpp src/vision.cpp src/haptics.cpp src/comms.cpp src/glove_system.cpp src/main.cpp"
if errorlevel 1 (
  echo BUILD FAILED
  exit /b 1
)
echo OK - build\visionglove.exe
echo Try: build\visionglove.exe --test
exit /b 0
