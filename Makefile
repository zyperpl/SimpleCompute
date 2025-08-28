.PHONY: all clean run example

all: libcompute.so

libcompute.so: compute.o
	cc -shared -o libcompute.so compute.o

compute.o: compute.c
	cc -fPIC -O3 -c compute.c

example: example/example
	
example/example: example/main.cpp libcompute.so
	g++ -ggdb -lGL -lGLEW -lraylib example/main.cpp -L. -lcompute -o example/example

run: example/example
	cd example && LD_LIBRARY_PATH=.. ./example

clean:
	rm -f libcompute.so compute.o
	rm -f example/example
