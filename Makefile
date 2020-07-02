CFLAGS=-std=c11 -g -static
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

ynicc: $(OBJS)
	$(CC) -o ynicc $(OBJS) $(CFLAGS)


$(OBJS): ynicc.h


test: ynicc
	./test.sh

clean:
	rm -f ynicc *.o *~ tmp*

.PHONY: test clean

