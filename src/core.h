#pragma once

#if defined(NDEBUG)
  #define ENG_RELEASE
#else
  #define ENG_DEBUG
#endif


#if defined(ENG_DEBUG)
  #define ENG_ASSERTION_ENABLED
#endif

#if defined(ENG_DEBUG)
  #define ENG_LOGGING_ENABLED
#endif


#if defined(_MSC_VER)
  #define ENG_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__)
  #define ENG_DEBUG_BREAK() __builtin_trap()
#else
  #error Currently, only MSVC and Clang are supported
#endif


#if defined(_MSC_VER) && !defined(__INLINING__)
  #define ENG_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
  #if __cplusplus >= 201703L
    #define ENG_FORCE_INLINE [[nodiscard]] __attribute__((always_inline))
  #else
    #define ENG_FORCE_INLINE __attribute__((always_inline))
  #endif
#else
  #define ENG_FORCE_INLINE inline
#endif


#if defined(ENG_DEBUG)
    #define ENG_ASSERT_MSG(COND, FMT, ...)                  \
        if (!(COND)) {                                      \
            fmt::print(FMT, __VA_ARGS__);                   \
            fmt::print(", {} ({})\n", __FILE__, __LINE__);  \
            ENG_DEBUG_BREAK();                              \
        }

    #define ENG_ASSERT(COND)          ENG_ASSERT_MSG(COND, #COND)
    #define ENG_ASSERT_FAIL(FMT, ...) ENG_ASSERT_MSG(false, FMT, __VA_ARGS__)
        
#else
    #define ENG_ASSERT_MSG(COND, FMT, ...)
    #define ENG_ASSERT(COND)
    #define ENG_ASSERT_FAIL(FMT, ...)
#endif


#define ENG_PRAGMA_OPTIMIZE_OFF _Pragma("optimize(\"\", off)")
#define ENG_PRAGMA_OPTIMIZE_ON  _Pragma("optimize(\"\", on)")
