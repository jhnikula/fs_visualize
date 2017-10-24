#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <png.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#include <mm_malloc.h>
#else
#define _mm_malloc(s, a)	malloc(s)
#define _mm_free		free
#endif

extern unsigned char avg_sse2(unsigned char *buf, int len);

#define MAX_PIXELS	1024*1024
#define MAX_FILE_NAME	256
#define MAX_CACHING	8*1024*1024

unsigned long calc_pix_size(off_t fsize)
{
	unsigned long psize = sysconf(_SC_PAGESIZE);

	while ((fsize + psize - 1) / psize > MAX_PIXELS)
		psize <<= 1;

	return psize;
}

unsigned char avg_generic(unsigned char *buf, int len)
{
	unsigned int sum = 0;
	int i = len;

	while (i--)
		sum += *buf++;

	return sum / len;
}

/*
 * This function is originally from
 * http://www.labbookpages.co.uk/software/imgProc/libPNG.html
 */
int writeImage(char* filename, int width, int height, int *buffer, bool color)
{
	int code = 0;
	FILE *fp;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_bytep row = NULL;
	
	// Open file for writing (binary mode)
	fp = fopen(filename, "wb");
	if (fp == NULL) {
		fprintf(stderr, "Could not open file %s for writing\n", filename);
		code = 1;
		goto finalise;
	}

	// Initialize write structure
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		fprintf(stderr, "Could not allocate write struct\n");
		code = 1;
		goto finalise;
	}

	// Initialize info structure
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		fprintf(stderr, "Could not allocate info struct\n");
		code = 1;
		goto finalise;
	}

	// Setup Exception handling
	if (setjmp(png_jmpbuf(png_ptr))) {
		fprintf(stderr, "Error during png creation\n");
		code = 1;
		goto finalise;
	}

	png_init_io(png_ptr, fp);

	// Write header (8 bit gray)
	png_set_IHDR(png_ptr, info_ptr, width, height, 8,
			color ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_GRAY,
			PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);

	// Allocate memory for one row
	row = (png_bytep) malloc((color ? 3 : 1) * width * sizeof(png_byte));
	if (row == NULL) {
		fprintf(stderr, "Error allocating memory\n");
		code = 1;
		goto finalise;
	}

	// Write image data
	int x, y, pixel;
	for (y=0 ; y<height ; y++) {
		for (x=0 ; x<width ; x++) {
			pixel = buffer[y*width + x];
			if (color == false) {
				row[x] = pixel;
			} else if (pixel >= 0) {
				row[3 * x] = pixel;
				row[3 * x + 1] = pixel;
				row[3 * x + 2] = pixel;
			} else {
				row[3 * x] = -pixel;
				row[3 * x + 1] = 0;
				row[3 * x + 2] = 0;
			}
		}
		png_write_row(png_ptr, row);
	}

	// End write
	png_write_end(png_ptr, NULL);

finalise:
	if (fp != NULL) fclose(fp);
	if (info_ptr != NULL) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
	if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
	free(row);

	return code;
}

int main(int argc, char **argv)
{
	int fp;
	off_t len, progress_thr = 0, processed = 0;
	unsigned long pix_size;
	unsigned char *buf;
	bool color = false;
	int i, pixels, w, pos = 0, cached = 0, *img;
	time_t t;
	char outf_name[MAX_FILE_NAME];
	unsigned char (*avg)(unsigned char *buf, int len) = avg_generic;
#if defined(__i386__) || defined(__x86_64__)
	unsigned int eax, ebx, ecx, edx = 0;
#endif

	if (argc < 2 || argc > 3) {
		printf("Usage: %s <source_file> [<target_file>]\n", argv[0]);
		return 0;
	}

	if (argc > 2) {
		outf_name[MAX_FILE_NAME - 1] = '\0';
		strncpy(outf_name, argv[2], MAX_FILE_NAME - 1);
	}

	fp = open(argv[1], O_RDONLY);
	if (fp < 0) {
		perror(argv[1]);
		goto err1;
	}

	len = lseek(fp, 0L, SEEK_END);
	if (len < 0) {
		perror(argv[1]);
		goto err2;
	}
	lseek(fp, 0L, SEEK_SET);
	pix_size = calc_pix_size(len);

	buf = _mm_malloc(pix_size, 16);
	if (buf == NULL) {
		perror("");
		goto err2;
	}

#if defined(__i386__) || defined(__x86_64__)
	__get_cpuid(1, &eax, &ebx, &ecx, &edx);
	if (edx & bit_SSE2) {
		printf("Using SSE2 optimizations\n");
		avg = avg_sse2;
	}
#endif

	pixels = (len + pix_size - 1) / pix_size;
	w = ceil(sqrt(pixels));

	fprintf(stderr, "len %lld, pix_size %lu, pixels %d, w %d\n",
		(long long int)len, pix_size, pixels, w);
	img = malloc(sizeof(*img) * w * w);
	if (img == NULL) {
		perror("");
		goto err3;
	}

	while (len - processed > pix_size) {
		i = read(fp, buf, pix_size);
		/*
		 * calculate byte average using optimized algorithm found
		 * above or in case of error indicate it with negative value
		 */
		processed += pix_size;
		if (i == pix_size) {
			img[pos++] = (*avg)(buf, i);
		} else {
			fprintf(stderr, "\nRead error at %lld. Error %d. %s\n",
				(long long int)processed - pix_size,
				errno, strerror(errno));
			img[pos++] = -255;
			color = true;
			/*
			 * File offset update is undefined after read error.
			 * Seek exactly at the next chunk.
			 */
			lseek(fp, processed, SEEK_SET);
		}

		cached += i;
		if (cached >= MAX_CACHING) {
			posix_fadvise(fp, processed - cached, cached,
				      POSIX_FADV_DONTNEED);
			cached = 0;
		}

		if (processed > progress_thr) {
			progress_thr += len / 100;
			printf("\r%d %%", (int)(processed / (len / 100)));
			fflush(stdout);
		}
	}

	/*
	 * calculate remaining bytes. Have to use generic byte average
	 * algorithm since optimized algorithms are not expected to be able
	 * to deal with data that is not multiple of PAGE_SIZE
	 */
	if (len - processed > 0) {
		i = read(fp, buf, len - processed);
		if (i == (len - processed)) {
			img[pos] = avg_generic(buf, i);
		} else {
			fprintf(stderr, "\nRead error at %lld. Error %d. %s\n",
				(long long int)processed,
				errno, strerror(errno));
			img[pos] = -255;
			color = true;
		}
	}

	puts("\r100 %");

	if (argc < 3) {
		t = time(NULL);
		strftime(outf_name, sizeof(outf_name), "%Y%m%d-%H%M%S.png", localtime(&t));
	}

	writeImage(outf_name, w, w, img, color);

	free(img);
	_mm_free(buf);
	close(fp);

	return 0;
err3:
	_mm_free(buf);
err2:
	if (fp)
		close(fp);
err1:
	return 1;
}
