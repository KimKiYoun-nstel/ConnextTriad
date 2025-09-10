#pragma once
#ifdef _WIN32
#if defined(PARSER_BUILD_SHARED)  // DLL 빌드 중
#define PARSER_API __declspec(dllexport)
#elif defined(PARSER_USE_SHARED)  // DLL import 사용(정적X, 로더X)
#define PARSER_API __declspec(dllimport)
#else  // 정적 링크 또는 로더 사용
#define PARSER_API
#endif
#else
#define PARSER_API
#endif

extern "C" {
PARSER_API int parser_api_version();
PARSER_API bool parser_from_json(const char*, const char*, char*, size_t*);
PARSER_API bool parser_to_json(const char*, const char*, char*, size_t*);
}
