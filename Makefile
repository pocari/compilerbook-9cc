CFLAGS=-std=c11 -O0 -g -static
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

ynicc: $(OBJS)
	$(CC) -o ynicc $(OBJS) $(CFLAGS)


$(OBJS): ynicc.h


test: ynicc
	./ynicc tests > tmp.s
	gcc -O0 -c test_func.c
	gcc -O0 -static -o tmp test_func.o tmp.s
	./tmp

ynicc-gen2: ynicc
	./self.sh

test-gen2: ynicc-gen2
	./ynicc-gen2 tests > tmp.s
	gcc -O0 -c test_func.c
	gcc -O0 -static -o tmp test_func.o tmp.s
	./tmp

clean:
	rm -rf tmp-self
	rm -rf tmp-self3
	rm -f ynicc *.o *~ tmp*

.PHONY: test clean

