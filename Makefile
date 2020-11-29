CC = gcc
CFLAGS = -Wall

all: detector

detector: Asst2.c
	$(CC) $(CFLAGS) Asst2.c -o detector -pthread -lm

# For debugging
debug: CFLAGS = -Wall -g -fsanitize=address,undefined -D DEBUG=1
debug: all

 
testing: CFLAGS = -D DEBUG=1
testing: all

clean:
	rm -f a.out detector Asst2 *.o


