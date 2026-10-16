@echo off
rem Rebuilds display.exe from display.cpp plus the prebuilt obj\*.o
setlocal enabledelayedexpansion
if not exist obj\implot_items.o call build_libs.bat || exit /b 1
set OBJS=
for %%o in (obj\*.o) do set OBJS=!OBJS! %%o
g++ -g -Wall -Wextra -std=c++17 -I include -I third_party\imgui-master ^
  -I third_party\imgui-master\backends -I third_party\implot ^
  display.cpp !OBJS! -o display.exe -lglfw3 -lopengl32 -lgdi32