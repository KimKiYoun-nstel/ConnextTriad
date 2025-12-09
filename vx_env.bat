@echo off

REM ==== 1) wrenv.exe 위치 확인해서 맞게 수정 ==== 
REM 보통은 D:\WindRiver\wrenv\wrenv.exe 이런 식임
REM 아래 경로가 실제로 존재하는지 먼저 탐색기에서 확인해봐
set WRENV=D:\WindRiver\wrenv.exe

if not exist "%WRENV%" (
    echo [ERROR] wrenv.exe not found at "%WRENV%"
    pause
    exit /b 1
)

REM ==== 2) VxWorks 환경이 적용된 cmd 창을 연다 ====
REM   -p vxworks-23.03 부분도 설치된 프로파일 이름에 맞게 필요하면 수정
REM   환경 변수 설정을 위한 임시 배치 파일을 생성하여 실행합니다.
REM   이렇게 하면 복잡한 인용부호나 줄바꿈 문제를 피할 수 있습니다.

set "INIT_BAT=%TEMP%\vx_init_%RANDOM%.bat"

(
    echo @echo off
    echo set VSB_DIR=D:\WindRiver\workspace_test
    echo set WIND_CC_SYSROOT=D:\WindRiver\workspace_test
    echo set WIND_BASE=D:\WindRiver\vxworks\23.03
    echo set NDDSHOME_CTL=D:\rti_connext_dds-7.3.1
    echo set CLANGXX=D:\WindRiver\compilers\llvm-15.0.0.1\WIN64\bin\clang++.exe
    echo set LLVM_ROOT=D:\WindRiver\compilers\llvm-15.0.0.1\WIN64
    echo title VxWorks Dev Shell
    echo echo [INFO] VxWorks Environment Configured.
) > "%INIT_BAT%"

REM   임시 배치 파일을 실행하고 나서 삭제하도록 합니다.
"%WRENV%" cmd /k "call "%INIT_BAT%" & del "%INIT_BAT%""
