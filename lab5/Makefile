CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200112L
LDFLAGS = -lm

TARGETS = rdt_sender rdt_receiver

all: $(TARGETS)

rdt_sender: rdt_sender.o rdt_common.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

rdt_receiver: rdt_receiver.o rdt_common.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c rdt_common.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGETS)

.PHONY: all clean
