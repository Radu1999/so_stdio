build: so_stdio.o
	gcc -shared so_stdio.o -o libso_stdio.so
	
so_stdio.o:
	gcc -c so_stdio.c

clean:
	rm *.o libso_stdio.so
