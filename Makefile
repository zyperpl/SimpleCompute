.PHONY: all clean run example

all: libcompute.a

libcompute.a: compute.o
	ar rcs libcompute.a compute.o

compute.o: compute.c
	cc -fPIC -O3 -c compute.c

example: example/example
	
example/example: example/main.cpp libcompute.a
	g++ -ggdb -lGL -lGLEW -lraylib example/main.cpp -L. -lcompute -o example/example

run: example/example
	cd example && ./example

clean:
	rm -f libcompute.a compute.o
	rm -f example/example
