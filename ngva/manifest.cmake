# NGVA IDL 목록과 "파일 기준 타입명"을 선언하면
# 생성된 .cxx를 RtpDdsCore에 자동 편입합니다.
# (IDL에 여러 타입이 있는 경우 파일명과 동일한 주요 타입만 대상)
#
# 예시:
# set(NGVA_IDL_LIST
#   "${CMAKE_SOURCE_DIR}/ngva/idl/NgvaArbitration.idl"
#   "${CMAKE_SOURCE_DIR}/ngva/idl/NgvaRegistry.idl"
# )
