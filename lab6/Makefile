CC = gcc
CFLAGS = -Wall -Wextra -O2

TARGETS = udp_pinger_client udp_pinger_server

all: $(TARGETS)

udp_pinger_client: udp_pinger_client.c
	$(CC) $(CFLAGS) -o $@ $<

udp_pinger_server: udp_pinger_server.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)
