CFLAGS=-std=c11 -g -static
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

ynicc: $(OBJS)
	$(CC) -o ynicc $(OBJS) $(CFLAGS)


$(OBJS): ynicc.h


test: ynicc
	./ynicc tests > tmp.s
	gcc -c test_func.c
	gcc -no-pie -o tmp test_func.o tmp.s
	./tmp

clean:
	rm -f ynicc *.o *~ tmp*

.PHONY: test clean

