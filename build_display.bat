@echo off
rem build_display.bat: builds display.exe in one step.
rem   build_display.bat         - compile ImGui/ImPlot once into obj\, then build display.exe
rem   build_display.bat clean   - recompile ImGui/ImPlot too (after updating either library)
cd /d "%~dp0"
setlocal enabledelayedexpansion

if /i "%1"=="clean" if exist obj rmdir /s /q obj

if not exist obj\implot_items.o (
  echo Compiling ImGui and ImPlot into obj\ ^(first run only, slow^)
  if not exist obj mkdir obj
  for %%f in (third_party\imgui-master\*.cpp ^
              third_party\imgui-master\backends\imgui_impl_glfw.cpp ^
              third_party\imgui-master\backends\imgui_impl_opengl3.cpp ^
              third_party\implot\*.cpp) do (
    echo   %%~nxf
    g++ -c -g -std=c++17 -I third_party\imgui-master -I third_party\implot "%%f" -o "obj\%%~nf.o" || exit /b 1
  )
)

echo Building display.exe
set OBJS=
for %%o in (obj\*.o) do set OBJS=!OBJS! %%o
g++ -g -Wall -Wextra -std=c++17 -I include -I third_party\imgui-master ^
  -I third_party\imgui-master\backends -I third_party\implot ^
  display.cpp !OBJS! -o display.exe -lglfw3 -lopengl32 -lgdi32 || exit /b 1
echo Done: display.exe