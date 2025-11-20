# Ayumi - AY-3-8910/YM2149 emulator
# Build utilities for testing and benchmarking

CC = gcc
CFLAGS = -Wall -Wextra -pedantic
CFLAGS_RELEASE = $(CFLAGS) -O3
LIBS = -lm

.PHONY: all test bench clean

all: test bench

# Test utility - verifies all output modes work correctly
test: test_ayumi
	./test_ayumi

test_ayumi: test_ayumi.c ayumi.c ayumi.h
	$(CC) $(CFLAGS) test_ayumi.c ayumi.c $(LIBS) -o test_ayumi

# Benchmark utility - measures performance of all output modes
bench: bench_ayumi
	./bench_ayumi

bench_ayumi: bench_ayumi.c ayumi.c ayumi.h
	$(CC) $(CFLAGS_RELEASE) bench_ayumi.c ayumi.c $(LIBS) -o bench_ayumi

clean:
	rm -f test_ayumi bench_ayumi *.o
