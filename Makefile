.PHONY: all clean run

all: libcompute.a example

libcompute.a: compute.o
	ar rcs libcompute.a compute.o

compute.o: compute.c
	cc -fPIC -O3 -c compute.c

example: main.cpp libcompute.a
	g++ -ggdb -lGL -lGLEW -lraylib main.cpp -L. -lcompute -o example

run: example
	./example

clean:
	rm -f libcompute.a compute.o
	rm -f example
