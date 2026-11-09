@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (echo VCVARS FAILED & exit /b 1)
cd /d "E:\202691\eda-work"
if not exist build mkdir build
cl /nologo /c /std:c++20 /EHsc /MDd /D_DEBUG /D_UNICODE /DUNICODE /D__WXMSW__ /DNOMINMAX /utf-8 /W3 ^
  /I"D:\wxWidgets-3.3.3\include" /I"D:\wxWidgets-3.3.3\include\msvc" /I"D:\wxWidgets-3.3.3\lib\vc_x64_lib\mswud" ^
  /Fo:build\ /Fd:build\eda.pdb /FC src\main.cpp
if errorlevel 1 (echo COMPILE FAILED & exit /b 1)
link /nologo /OUT:build\EDA.exe /SUBSYSTEM:WINDOWS /DEBUG /PDB:build\EDA.pdb /LIBPATH:"D:\wxWidgets-3.3.3\lib\vc_x64_lib" build\main.obj
if errorlevel 1 (echo LINK FAILED & exit /b 1)
echo BUILD OK
