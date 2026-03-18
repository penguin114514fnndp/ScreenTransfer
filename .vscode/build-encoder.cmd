@echo off
setlocal

:: ================= [项目配置区] =================
set "ROOT=%~dp0.."
set "WORKSPACE=%ROOT%\Encoder"
set "INCLUDES=%WORKSPACE%\include"
set "SRC=%WORKSPACE%\src\*.cpp"
set "OUT_EXE=%ROOT%\output\qr_video_encoder.exe"
set "OBJ_DIR=%ROOT%\output\.obj_temp_encoder"
:: ===============================================

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%i"
if not defined VS_PATH (
  echo [ERROR] No Visual Studio C++ toolchain found.
  exit /b 1
)

call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul

if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

echo Compiling Encoder...
cl /nologo /utf-8 /EHsc /std:c++17 /O2 /W3 ^
  /I"%INCLUDES%" "%SRC%" /Fo"%OBJ_DIR%\\" ^
  /link /OUT:"%OUT_EXE%"

set "CL_EXIT=%ERRORLEVEL%"
if not "%CL_EXIT%"=="0" (
  echo [ERROR] Encoder build failed.
  if exist "%OBJ_DIR%" rmdir /s /q "%OBJ_DIR%"
  exit /b %CL_EXIT%
)

if exist "%OBJ_DIR%" rmdir /s /q "%OBJ_DIR%"

echo [SUCCESS] Encoder build succeeded.
exit /b 0
