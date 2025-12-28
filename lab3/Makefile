CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2
TARGET = smtp_client
SRCS = smtp_client.c smtp_utils.c mime_utils.c tls_utils.c
LIBS = -lssl -lcrypto

OPENSSL_AVAILABLE := $(shell pkg-config --exists openssl && echo YES)

ifeq ($(OPENSSL_AVAILABLE),YES)
  CFLAGS += -DUSE_OPENSSL
  TLS_LIBS = $(LIBS)
else
  TLS_LIBS = 
endif

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(TLS_LIBS)

debug: CFLAGS += -DDEBUG -g
debug: $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all debug clean