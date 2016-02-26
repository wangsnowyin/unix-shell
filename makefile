CC=gcc

shell.out: shell.c
	$(CC) shell.c -o gosh

clean:
	@rm -f *.out *.o gosh results.log
