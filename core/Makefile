# Makefile — KernelViz C Core
# Compiles all OS simulation modules into a shared library: libkernelviz.so
#
# Usage:
#   make          → build libkernelviz.so
#   make clean    → remove build artifacts
#   make test     → build + run a quick smoke test

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -fPIC -O2 -I.
LDFLAGS = -shared -lm

SRCS    = kernel.c scheduler.c memory.c fs.c ipc.c process.c
OBJS    = $(SRCS:.c=.o)
TARGET  = libkernelviz.so

.PHONY: all clean test

all: $(TARGET)
	@echo "[OK] Built $(TARGET)"

# Link all object files into shared library
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# Compile individual translation units
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Header dependencies
kernel.o:    kernel.c kernel.h process.h scheduler.h memory.h fs.h ipc.h
scheduler.o: scheduler.c scheduler.h process.h
memory.o:    memory.c memory.h
fs.o:        fs.c fs.h
ipc.o:       ipc.c ipc.h
process.o:   process.c process.h

clean:
	rm -f $(OBJS) $(TARGET)
	@echo "[OK] Cleaned build artifacts"

# Quick smoke test: compile a tiny driver, run it, expect JSON output
test: $(TARGET)
	$(CC) -std=c99 -o _test_kernel _test_kernel.c -L. -lkernelviz -Wl,-rpath,.
	LD_LIBRARY_PATH=. ./_test_kernel
	rm -f _test_kernel
