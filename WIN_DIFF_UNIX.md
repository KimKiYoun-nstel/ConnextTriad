WIN_DIFF_UNIX.md

This file summarizes the changes made to allow building on Linux while preserving Windows behavior.
All explanations and comments are written in Korean as requested. The file lists files changed, a short
rationale, and the pattern used for platform branching.

Summary of edited files and key changes

1) CMakeLists.txt (root)
   - 추가: if(UNIX) add_compile_definitions(RTI_UNIX=1)
   - 이유: RTI 헤더와 rtiddsgen이 올바른 플랫폼 분기를 사용하도록 보장함. Windows 빌드에는
     영향을 주지 않음.
   - 체크: Windows에서 configure할 때 `RTI_UNIX`가 정의되지 않아야 함.

2) IdlKit/CMakeLists.txt
   - 초기 디버그에서는 대상별로 RTI_UNIX를 설정했으나, 최종적으로는 루트에서 전역 설정으로
     대체되었습니다. (IdlKit 쪽 변경은 제거되었거나 주석 처리 가능)

3) DkmRtpIpc/CMakeLists.txt
   - 변경: target_link_libraries(... ws2_32 ...) 호출을 `if(WIN32)`로 감쌌습니다.
   - 이유: Linux에서 winsock 라이브러리를 링크하려고 할 때 에러가 발생하지 않도록 하기 위함.

4) DkmRtpIpc/include/triad_log.hpp
   - 변경: Windows에서는 `localtime_s`를 사용하고, POSIX에서는 `localtime_r`를 사용하도록 분기.
   - 코드 패턴:
     #ifdef _WIN32
       localtime_s(...)
     #else
       localtime_r(...)
     #endif

5) DkmRtpIpc/src/dkmrtp_ipc.cpp
   - 주요 변경: Winsock 기반 코드와 POSIX 소켓 코드를 `_WIN32` 분기로 분리.
   - 추가: POSIX에서 `SOCKET` 타입과 `INVALID_SOCKET` 등의 매핑을 제공. `close()` 사용을 적용.
   - 추가: 64비트 바이트오더 변환 헬퍼(`htonll` / `ntohll`)를 추가하여 Linux에서의 누락을 보완.
   - 네트워크 I/O의 작은 차이(예: select 인자, send/recv 반환값)의 플랫폼 차이를 분기하여 처리.

6) RtpDdsGateway/src/rti_logger_bridge.cpp
   - 변경: 일부 함수 선언/정의에서 명시적 네임스페이스 한정자가 컴파일러별로 충돌을 일으켜 간단히
     함수 이름을 네임스페이스 내부에 정의하도록 변경(기능 변경 없음).

7) RtpDdsGateway/include/async/async_event_processor.hpp + .cpp
   - 변경: 일부 생성자 위임/기본 생성자 추가로 컴파일러 간 초기화 차이 제거.

패턴과 권장사항

- CMake 분기
  if(WIN32)
    # windows-only 링크/설정
  else()
    # unix-only 설정
  endif()

- C/C++ 전처리 분기
  #ifdef _WIN32
    // Windows includes / helpers
  #else
    // POSIX includes / helpers
  #endif

- 변경은 가능한 한 원본 Windows 코드를 보존하고, POSIX 대체 코드를 `#else` 분기 아래에 추가하였습니다.

검증 포인트 (윈도우 빌드 유지 확인)

- Windows에서 configure 시 `RTI_UNIX`가 정의되어 있지 않아야 함.
- Windows 빌드에서 `ws2_32`가 링크되는지 확인.
- Linux에서 winsock 헤더(`winsock2.h`) 포함으로 인한 빌드 실패가 없어야 함.

참고: 각 파일의 보다 구체적인 라인별 변경 내역(패치)은 Git history에서 확인하세요. 이 문서는
편의를 위해 변경 파일/핵심 스니펫/설명만을 제공합니다.
