LDFLAGS=../libsz.a

PROGS=t01 t02

.c:
	$(CC) $(CFLAGS) -o $* $*.c $(LDFLAGS)

default: all run-all

all:
	for i in $(PROGS) ; \
	do	make $$i ; \
	done

run-all:
	for i in $(PROGS) ; \
	do	./$$i ; \
	done

clean:
	rm -f $(PROGS)
