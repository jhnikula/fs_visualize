CC=gcc
CFLAGS=-O2 -Wall -D_FILE_OFFSET_BITS=64
LIBS=-lm -lpng

OUTPUT=fs_visualize

$(OUTPUT): fs_visualize.o
	$(CC) $(CFLAGS) $(LIBS) -o $(OUTPUT) fs_visualize.o

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f *.o $(OUTPUT)

