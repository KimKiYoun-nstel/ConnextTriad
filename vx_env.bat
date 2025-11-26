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
"%WRENV%" cmd /k "set WIND_CC_SYSROOT=D:\WindRiver\workspace_new\t2080_VSB_SMP& set WIND_BASE=D:\WindRiver\vxworks\23.03& set NDDSHOME_CTL=D:\rti_connext_dds-7.3.0& title VxWorks Dev Shell"
