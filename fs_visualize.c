#include <fcntl.h>
#include <math.h>
#include <png.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_PIXELS	1024*1024
#define MAX_FILE_NAME	256

unsigned long calc_pix_size(off_t fsize)
{
	unsigned long psize = sysconf(_SC_PAGESIZE);

	while ((fsize + psize - 1) / psize > MAX_PIXELS)
		psize <<= 1;

	return psize;
}

unsigned char avg(unsigned char *buf, int len)
{
	unsigned int sum = 0;
	int i = len;

	while (i--)
		sum += *buf++;

	return sum / len;
}

/*
 * This function is from
 * http://www.labbookpages.co.uk/software/imgProc/libPNG.html
 */
int writeImage(char* filename, int width, int height, unsigned char *buffer)
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
	png_set_IHDR(png_ptr, info_ptr, width, height,
			8, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);

	// Allocate memory for one row
	row = (png_bytep) malloc(width * sizeof(png_byte));
	if (row == NULL) {
		fprintf(stderr, "Error allocating memory\n");
		code = 1;
		goto finalise;
	}

	// Write image data
	int x, y;
	for (y=0 ; y<height ; y++) {
		for (x=0 ; x<width ; x++) {
			row[x] = buffer[y*width + x];
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
	off_t len, processed = 0, tmp;
	unsigned long pix_size;
	unsigned char *buf, *img;
	int i, pixels, w, pos = 0, progress_percent = -1;
	time_t t;
	char outf_name[MAX_FILE_NAME];

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

	buf = malloc(pix_size);
	if (buf == NULL) {
		perror("");
		goto err2;
	}

	pixels = (len + pix_size - 1) / pix_size;
	w = ceil(sqrt(pixels));

	fprintf(stderr, "len %lld, pix_size %lu, pixels %d, w %d\n",
		(long long int)len, pix_size, pixels, w);
	img = malloc(w * w);
	if (img == NULL) {
		perror("");
		goto err3;
	}

	while (len - processed > pix_size) {
		i = read(fp, buf, pix_size);
		img[pos++] = avg(buf, i);
		if (i != pix_size) {
			perror("");
			goto err4;
		}
		processed += i;

		tmp = processed / (len / 100);
		if (tmp > progress_percent) {
			progress_percent = tmp;
			printf("\r%d %%", progress_percent);
			fflush(stdout);
		}
	}

	if (len - processed > 0) {
		i = read(fp, buf, len - processed);
		img[pos] = avg(buf, i);
	}

	puts("\r100 %");

	if (argc < 3) {
		t = time(NULL);
		strftime(outf_name, sizeof(outf_name), "%Y%m%d-%H%M%S.png", localtime(&t));
	}

	writeImage(outf_name, w, w, img);

	free(img);
	free(buf);
	close(fp);

	return 0;
err4:
	free(img);
err3:
	free(buf);
err2:
	if (fp)
		close(fp);
err1:
	return 1;
}
