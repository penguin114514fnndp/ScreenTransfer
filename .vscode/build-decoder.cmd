@echo off
setlocal

:: ================= [项目配置区] =================
set "ROOT=%~dp0.."
set "WORKSPACE=%ROOT%\Decoder"
set "INCLUDES=%WORKSPACE%\include"
set "SRC=%WORKSPACE%\src\*.cpp"
set "LIB_DIR=%WORKSPACE%\lib"
set "LIBS=opencv_world4120.lib"
set "OUT_EXE=%ROOT%\output\qr_video_decoder.exe"
set "OBJ_DIR=%ROOT%\output\.obj_temp"
:: ===============================================

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%i"
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if not defined VS_PATH (
  echo [ERROR] No Visual Studio C++ toolchain found.
  exit /b 1
)

if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

echo Compiling...
cl /nologo /utf-8 /EHsc /std:c++17 ^
   /I"%INCLUDES%" "%SRC%" /Fo"%OBJ_DIR%\\" /link /LIBPATH:"%LIB_DIR%" "%LIBS%" /OUT:"%OUT_EXE%"

set "CL_EXIT=%ERRORLEVEL%"

if not "%CL_EXIT%"=="0" (
  echo [ERROR] Build failed.
  if exist "%OBJ_DIR%" rmdir /s /q "%OBJ_DIR%"
  exit /b %CL_EXIT%
)

if exist "%OBJ_DIR%" rmdir /s /q "%OBJ_DIR%"

echo [SUCCESS] Build succeeded.
exit /b 0