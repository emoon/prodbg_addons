#ifndef M68KTYPES_H 
#define M68KTYPES_H  

#if defined(_WIN32)

#define M68K_LIKELY(exp) exp 
#define M68K_UNLIKELY(exp) exp 
#define M68K_INLINE __forceinline
#define M68K_RESTRICT __restrict
#define M68K_ALIGN(x) __declspec(align(x))
#define M68K_ALIGNOF(t) __alignof(t)
#define M68K_BREAK __debugbreak()

#elif defined(__APPLE__)

#define M68K_LIKELY(exp) __builtin_expect(exp, 1) 
#define M68K_UNLIKELY(exp) __builtin_expect(exp, 0) 
#define M68K_INLINE inline
#define M68K_RESTRICT __restrict
#define M68K_ALIGN(x) __attribute__((aligned(x)))
#define M68K_ALIGNOF(t) __alignof__(t)
#define M68K_BREAK ((*(volatile uint32_t *)(0)) = 0x666)

#endif

#endif

