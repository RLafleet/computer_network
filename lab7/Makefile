CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
TARGET = proxy_server
SRC = proxy_server.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)
