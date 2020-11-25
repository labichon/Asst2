CC = gcc
CFLAGS = -Wall

all: detector

detector:
	$(CC) $(CFLAGS) Asst2.c -o detector

# For debugging
debug: CFLAGS = -Wall -g -fsanitize=address,undefined -D DEBUG=1
debug: all

clean:
	rm -f a.out Asst2 *.o


