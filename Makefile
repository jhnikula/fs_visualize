CC=gcc
ARCH=$(shell uname -m | sed -e s/i.86/x86/ -e s/x86_64/x86/)

CFLAGS=-O2 -Wall -D_FILE_OFFSET_BITS=64
LIBS=-lm -lpng

OUTPUT=fs_visualize
OBJS=fs_visualize.o
ifeq ($(ARCH),x86)
OBJS += avg_sse2.o
endif

$(OUTPUT): $(OBJS)
	$(CC) $(CFLAGS) $(LIBS) -o $(OUTPUT) $(OBJS)

avg_sse2.o: avg_sse2.c
	$(CC) $(CFLAGS) -msse2 -o $@ -c $<

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f *.o $(OUTPUT)

