setup:
	gcc -std=gnu99 -g -Wall -lm -o smallsh smallsh.c

clean:
	rm smallsh

test:
	./testscript > testresults.txt 2>&1