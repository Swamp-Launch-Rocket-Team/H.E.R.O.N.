@echo off
rem Compiles ImGui and ImPlot to obj\*.o. Slow, but only needed once.
if not exist obj mkdir obj
for %%f in (third_party\imgui-master\*.cpp ^
            third_party\imgui-master\backends\imgui_impl_glfw.cpp ^
            third_party\imgui-master\backends\imgui_impl_opengl3.cpp ^
            third_party\implot\*.cpp) do (
  echo Compiling %%~nxf
  g++ -c -g -std=c++17 -I third_party\imgui-master -I third_party\implot "%%f" -o "obj\%%~nf.o" || exit /b 1
)