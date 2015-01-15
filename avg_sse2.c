#include <x86intrin.h>

unsigned char avg_sse2(unsigned char *buf, int len)
{
	int i = len;
	__m128i a, b, zero, sum_vect;
	int __attribute__ ((aligned(16))) sum[4];

	zero = _mm_set1_epi32(0);
	sum_vect = zero;
	while (i) {
		i -= 16;

		/* load 16*8-bit vector */
		a = _mm_load_si128((__m128i *)buf);
		buf += 16;

		/*
		 * unpack low and high part of 16*8-bit vector as two 8*16-bit
		 * vectors and sum them
		 */
		b = a;
		b = _mm_unpackhi_epi8(b, zero);
		a = _mm_unpacklo_epi8(a, zero);
		a = _mm_add_epi16(a, b);

		/*
		 * unpack low and high part of 8*16-bit vector as two 4*32-bit
		 * vectors and sum them
		 */
		b = a;
		b = _mm_unpackhi_epi16(b, zero);
		a = _mm_unpacklo_epi16(a, zero);
		a = _mm_add_epi32(a, b);

		sum_vect = _mm_add_epi32(sum_vect, a);
	}
	_mm_store_si128((__m128i *)sum, sum_vect);

	return (sum[0] + sum[1] + sum[2] + sum[3]) / len;
}
