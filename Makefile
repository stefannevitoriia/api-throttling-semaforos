CC     = gcc
CFLAGS = -Wall -Wextra -pthread -Iinclude
COMUM  = src/throttling.c src/metricas.c

all: api_throttling http_throttling

api_throttling: src/main.c $(COMUM)
	$(CC) $(CFLAGS) -o $@ $^

http_throttling: src/http_throttling.c $(COMUM)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f api_throttling http_throttling

.PHONY: all clean
