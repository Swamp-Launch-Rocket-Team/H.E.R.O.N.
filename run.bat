@echo off
cd /d "%~dp0"
if "%~1"=="" (echo usage: run.bat demo  or  run.bat COM4 & exit /b 1)
where g++ >nul 2>nul ||  (echo g++ not found. Install MSYS2 and add C:\msys64\ucrt64\bin to PATH & exit /b 1)
call build_display.bat || exit /b 1
g++ -Wall -Wextra -std=c++17 -I include gcs.cpp -o gcs.exe || exit /b 1
g++ -Wall -Wextra -std=c++17 -I include fake_rx.cpp -o fake_rx.exe || exit /b 1
if /i "%~1"=="demo" (
  start "gcs" cmd /c "fake_rx.exe | gcs.exe"
) else (
  start "gcs" gcs.exe %1
)
timeout /t 1 >nul
start "" display.exe