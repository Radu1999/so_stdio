Nume: Chivereanu Radu-Gabriel
Grupă: 335 CA

# Tema 2


## Organizare

1. Solutia implementeaza functiile din fisierul so_stdio.h.

- Structura fisierului contine un buffer, 2 cursoare pentru citire/scriere, o structura enum pentru a putea retine ultima operatie facuta (read sau write), 2 flaguri pentru eroare/end_of_file si pid-ul procesului copil.
- Tema a fost utila intrucat m-a adus cu un nivel mai low decat eram obisnuit in general (exceptand IOCLA :) ).
- Consider implementarea fiind eficienta datorita bufferului pentru limitarea apelurilor de read/write.

## Implementare

- Intregul enunt al temei e implementat
- Executarea comenzii am facut o fara sh -c, parsand comanda si extragand fisierele de input/output daca exista.

## Cum se compilează și cum se rulează?

- Compilarea se face folosind make.
- Pentru a rula checkerul make & make all

## Bibliografie

- Laboratorul 3 SO - Procese
- Laboratorul 2 SO - Lucrul I/O

## Git

1. https://github.com/Radu1999/so_stdio
