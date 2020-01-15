CC=gcc

mcstatusc: mcstatusc.c
	$(CC) $^ -std=c99 -pedantic -Wall -Wextra -Werror -O2 -o$@

.PHONY: clean
clean:
	rm mcstatusc
