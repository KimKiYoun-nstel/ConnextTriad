@echo off
setlocal enabledelayedexpansion

rem ==========================================================
rem  VxWorks CMake build helper
rem  - workspace root에서 실행된다고 가정
rem  - toolchain: VxWorks\toolchain-vxworks-ppce6500.cmake
rem  - build dirs:
rem      out\build\vxworks\Debug
rem      out\build\vxworks\Release
rem ==========================================================

if "%~1"=="" goto :usage

set "ACTION=%~1"
set "WS=%CD%"
set "SRC_DIR=%WS%"

set "OUT_ROOT=%WS%\out\build\vxworks"
set "BD_DBG=%OUT_ROOT%\Debug"
set "BD_REL=%OUT_ROOT%\Release"
set "TC=%WS%\VxWorks\toolchain-vxworks-ppce6500.cmake"

if /I "%ACTION%"=="clean-debug"    goto :clean_debug
if /I "%ACTION%"=="clean-release"  goto :clean_release
if /I "%ACTION%"=="conf-debug"     goto :conf_debug
if /I "%ACTION%"=="conf-release"   goto :conf_release
if /I "%ACTION%"=="build-debug"    goto :build_debug
if /I "%ACTION%"=="build-release"  goto :build_release

:usage
echo Usage: %~nx0 ^<clean-debug^|clean-release^|conf-debug^|conf-release^|build-debug^|build-release^>
exit /b 2

:ensure_dir
set "DIR=%~1"
if not exist "%DIR%" mkdir "%DIR%"
goto :eof

:clean_debug
echo [vx_cmake_build] Removing "%BD_DBG%" ...
if exist "%BD_DBG%" rmdir /s /q "%BD_DBG%"
exit /b 0

:clean_release
echo [vx_cmake_build] Removing "%BD_REL%" ...
if exist "%BD_REL%" rmdir /s /q "%BD_REL%"
exit /b 0

:conf_debug
call :ensure_dir "%BD_DBG%"
cmake -S "%SRC_DIR%" -B "%BD_DBG%" -G Ninja ^
  -DCMAKE_TOOLCHAIN_FILE="%TC%" ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_CXX_FLAGS_DEBUG="-g" ^
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
exit /b %ERRORLEVEL%

:conf_release
call :ensure_dir "%BD_REL%"
cmake -S "%SRC_DIR%" -B "%BD_REL%" -G Ninja ^
  -DCMAKE_TOOLCHAIN_FILE="%TC%" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_CXX_FLAGS_RELEASE="-O2" ^
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
exit /b %ERRORLEVEL%

:build_debug
rem 항상 최신 Debug 설정으로 빌드하고 싶으면 conf-debug 먼저 호출
call "%~f0" conf-debug || exit /b %ERRORLEVEL%
cmake --build "%BD_DBG%" --target RtpDdsGateway
exit /b %ERRORLEVEL%

:build_release
rem 항상 최신 Release 설정으로 빌드하고 싶으면 conf-release 먼저 호출
call "%~f0" conf-release || exit /b %ERRORLEVEL%
cmake --build "%BD_REL%" --target RtpDdsGateway
exit /b %ERRORLEVEL%
