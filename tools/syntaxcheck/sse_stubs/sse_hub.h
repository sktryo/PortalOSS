/* Stub SSE/MMX headers for syntax-only checking on aarch64 hosts.
 *
 * The Source SDK mathlib headers include <xmmintrin.h> etc. unconditionally
 * (non-_X360) and declare inline functions that use SSE intrinsics. On an
 * aarch64 build host those intrinsics are unavailable and the closest thing is
 * a no-op stub so that -fsyntax-only passes. None of this is meant to run;
 * it only exists so the compiler can parse the SDK headers.
 */
#ifndef SSE2NEON_HUB_H
#define SSE2NEON_HUB_H

#include <stdint.h>
#include <string.h>

typedef float __m128 __attribute__((__vector_size__(16)));
typedef double __m128d __attribute__((__vector_size__(16)));
typedef long long __m128i __attribute__((__vector_size__(16)));
typedef long long __m64 __attribute__((__vector_size__(8)));

#define _MM_SHUFFLE(z,y,x,w) (((z)<<6)|((y)<<4)|((x)<<2)|(w))

static __inline__ __m128 _mm_set_ps(float e3, float e2, float e1, float e0) { __m128 r; float *f=(float*)&r; f[0]=e0; f[1]=e1; f[2]=e2; f[3]=e3; return r; }
static __inline__ __m128 _mm_set1_ps(float a) { return _mm_set_ps(a,a,a,a); }
static __inline__ __m128 _mm_set_ss(float a) { return _mm_set_ps(a,0.0f,0.0f,0.0f); }
static __inline__ __m128 _mm_setzero_ps(void) { return _mm_set1_ps(0.0f); }
static __inline__ __m128 _mm_load_ps(const float *p) { __m128 r; memcpy(&r,p,16); return r; }
static __inline__ __m128 _mm_loadu_ps(const float *p) { __m128 r; memcpy(&r,p,16); return r; }
static __inline__ __m128 _mm_load_ss(const float *p) { float v=*p; return _mm_set_ps(v,0,0,0); }
static __inline__ void _mm_store_ps(float *p, __m128 v) { memcpy(p,&v,16); }
static __inline__ void _mm_store_ss(float *p, __m128 v) { *p=((float*)&v)[0]; }
static __inline__ void _mm_storeu_ps(float *p, __m128 v) { memcpy(p,&v,16); }

static __inline__ __m128i _mm_setzero_si128(void) { __m128i r; memset(&r,0,16); return r; }
static __inline__ __m128i _mm_load_si128(const __m128i *p) { __m128i r; memcpy(&r,p,16); return r; }
static __inline__ __m128i _mm_loadu_si128(const __m128i *p) { __m128i r; memcpy(&r,p,16); return r; }
static __inline__ void _mm_store_si128(__m128i *p, __m128i v) { memcpy(p,&v,16); }
static __inline__ void _mm_storeu_si128(__m128i *p, __m128i v) { memcpy(p,&v,16); }

static __inline__ __m128 _mm_add_ps(__m128 a, __m128 b) { return a+b; }
static __inline__ __m128 _mm_sub_ps(__m128 a, __m128 b) { return a-b; }
static __inline__ __m128 _mm_mul_ps(__m128 a, __m128 b) { return a*b; }
static __inline__ __m128 _mm_div_ps(__m128 a, __m128 b) { return a/b; }
static __inline__ __m128 _mm_max_ps(__m128 a, __m128 b) { return a>b?a:b; }
static __inline__ __m128 _mm_min_ps(__m128 a, __m128 b) { return a<b?a:b; }
static __inline__ __m128 _mm_and_ps(__m128 a, __m128 b) { return a; }
static __inline__ __m128 _mm_andnot_ps(__m128 a, __m128 b) { return b; }
static __inline__ __m128 _mm_or_ps(__m128 a, __m128 b) { return a; }
static __inline__ __m128 _mm_xor_ps(__m128 a, __m128 b) { return a; }
static __inline__ __m128 _mm_sqrt_ps(__m128 a) { return a; }
static __inline__ __m128 _mm_rcp_ps(__m128 a) { return a; }
static __inline__ __m128 _mm_rsqrt_ps(__m128 a) { return a; }
static __inline__ __m128 _mm_movehl_ps(__m128 a, __m128 b) { return a; }
static __inline__ __m128 _mm_shuffle_ps(__m128 a, __m128 b, int m) { return a; }
#define _MM_TRANSPOSE4_PS(row0, row1, row2, row3) do { \
	__m128 tmp3, tmpa, tmpb; (void)tmp3; (void)tmpa; (void)tmpb; \
	(row1)=row1; (row2)=row2; (row3)=row3; } while(0)
static __inline__ void _mm_stream_ps(float *p, __m128 a) { (void)p; (void)a; }
static __inline__ void _mm_sfence(void) {}

static __inline__ __m128 _mm_add_ss(__m128 a, __m128 b) { return a+b; }
static __inline__ __m128 _mm_sub_ss(__m128 a, __m128 b) { return a-b; }
static __inline__ __m128 _mm_mul_ss(__m128 a, __m128 b) { return a*b; }
static __inline__ __m128 _mm_max_ss(__m128 a, __m128 b) { return a>b?a:b; }
static __inline__ __m128 _mm_min_ss(__m128 a, __m128 b) { return a<b?a:b; }
static __inline__ __m128 _mm_rcp_ss(__m128 a) { return a; }
static __inline__ __m128 _mm_rsqrt_ss(__m128 a) { return a; }
static __inline__ float _mm_cvtss_f32(__m128 a) { return ((float*)&a)[0]; }
static __inline__ int _mm_cvt_ss2si(__m128 a) { return (int)((float*)&a)[0]; }
static __inline__ int _mm_cvtss_si32(__m128 a) { return (int)((float*)&a)[0]; }
static __inline__ int _mm_cvttss_si32(__m128 a) { return (int)((float*)&a)[0]; }
static __inline__ __m64 _mm_cvtps_pi32(__m128 a) { __m64 r; memset(&r,0,8); return r; }
static __inline__ __m64 _mm_cvttps_pi32(__m128 a) { __m64 r; memset(&r,0,8); return r; }
static __inline__ __m128 _mm_cvtsi32_ss(__m128 a, int b) { ((float*)&a)[0]=(float)b; return a; }
static __inline__ __m128 _mm_cvt_si2ss(__m128 a, int b) { return _mm_cvtsi32_ss(a,b); }
static __inline__ __m128 _mm_cvtepi32_ps(__m128i a) { return _mm_set1_ps(0.0f); }
static __inline__ void _mm_empty(void) {}
static __inline__ int _mm_movemask_ps(__m128 a) { (void)a; return 0; }
static __inline__ int _mm_comigt_ss(__m128 a, __m128 b) { return ((float*)&a)[0]>((float*)&b)[0]; }
static __inline__ int _mm_comilt_ss(__m128 a, __m128 b) { return ((float*)&a)[0]<((float*)&b)[0]; }
static __inline__ __m128 _mm_cmpeq_ps(__m128 a, __m128 b) { return a==b; }
static __inline__ __m128 _mm_cmpgt_ps(__m128 a, __m128 b) { return a>b; }
static __inline__ __m128 _mm_cmpge_ps(__m128 a, __m128 b) { return a>=b; }
static __inline__ __m128 _mm_cmplt_ps(__m128 a, __m128 b) { return a<b; }
static __inline__ __m128 _mm_cmple_ps(__m128 a, __m128 b) { return a<=b; }

#endif
