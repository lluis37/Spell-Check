CC = gcc
CFLAGS = -g -Wall -std=c99 -O2

spchk: spchk.c spchk.h
	$(CC) $(CFLAGS) spchk.c -o spchk

test: spchk
	./spchk /usr/share/dict/words test.txt

test2: spchk
	chmod 000 test2.txt
	./spchk /usr/share/dict/words test2.txt
	
testdirectory: spchk
	./spchk /usr/share/dict/words testdirectory

macdonaldtest: spchk
	./spchk MacDonaldDict MacDonald.txt

clean:
	rm -f spchk