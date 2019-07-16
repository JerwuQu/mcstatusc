.PHONY: clean

mcstatusc: mcstatusc.c
	gcc mcstatusc.c -std=c99 -pedantic -Wall -Werror -O2 -omcstatusc

clean:
	rm mcstatusc
