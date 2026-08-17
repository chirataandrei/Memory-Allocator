UTILS_PATH ?= ../../utils
SRC_PATH ?= ../../src

CC = gcc
CPPFLAGS = -I$(UTILS_PATH)
CFLAGS = -Wall -Wextra -g
LDFLAGS = -L$(SRC_PATH)
LDLIBS = -losmem

.PHONY: run clean

run: test_local
	DYLD_LIBRARY_PATH=$(SRC_PATH) LD_LIBRARY_PATH=$(SRC_PATH) ./test_local

test_local: test_local.c
	$(MAKE) -C $(SRC_PATH)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf test_local test_local.dSYM
