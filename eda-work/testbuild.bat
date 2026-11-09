@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (echo VCVARS FAILED & exit /b 1)
cd /d "E:\202691\eda-work"
if not exist build mkdir build
cl /nologo /std:c++20 /EHsc /MDd /D_DEBUG /D_UNICODE /DUNICODE /D__WXMSW__ /DNOMINMAX /utf-8 /W3 /permissive- ^
  /I"D:\wxWidgets-3.3.3\include" /I"D:\wxWidgets-3.3.3\include\msvc" /I"D:\wxWidgets-3.3.3\lib\vc_x64_lib\mswud" ^
  /Fo:build\test\ /Fd:build\test.pdb /FC test\model_test.cpp ^
  /Fe:build\model_test.exe ^
  /link /SUBSYSTEM:CONSOLE /LIBPATH:"D:\wxWidgets-3.3.3\lib\vc_x64_lib"
if errorlevel 1 (echo TEST BUILD FAILED & exit /b 1)
echo TEST BUILD OK
