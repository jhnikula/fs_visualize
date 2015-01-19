#include <x86intrin.h>

unsigned char avg_sse2(unsigned char *buf, int len)
{
	int i = len, j;
	__m128i a, b, zero, sum_vect16, sum_vect32;
	int __attribute__ ((aligned(16))) sum[4];

	zero = _mm_set1_epi32(0);
	sum_vect32 = zero;
	while (i) {
		i -= 2048;
		sum_vect16 = zero;
		for (j = 0; j < 128; j++) {
			/* load 16*8-bit vector */
			a = _mm_load_si128((__m128i *)buf);
			buf += 16;

			/*
			 * unpack low and high part of 16*8-bit vector as two
			 * 8*16-bit vectors and sum them
			 */
			b = a;
			b = _mm_unpackhi_epi8(b, zero);
			a = _mm_unpacklo_epi8(a, zero);
			sum_vect16 = _mm_add_epi16(sum_vect16, b);
			sum_vect16 = _mm_add_epi16(sum_vect16, a);
		}
		/*
		 * unpack low and high part of 8*16-bit vector as two 4*32-bit
		 * vectors and sum them
		 */
		a = sum_vect16;
		b = a;
		b = _mm_unpackhi_epi16(b, zero);
		a = _mm_unpacklo_epi16(a, zero);
		sum_vect32 = _mm_add_epi32(sum_vect32, b);
		sum_vect32 = _mm_add_epi32(sum_vect32, a);
	}
	_mm_store_si128((__m128i *)sum, sum_vect32);

	return (sum[0] + sum[1] + sum[2] + sum[3]) / len;
}
