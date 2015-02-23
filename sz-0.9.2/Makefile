CATMAN=true
CC=cc
# Original: CFLAGS=-O3 -I$(INCLUDEDIR) -pipe -ansi -pedantic -Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS=-g -I$(INCLUDEDIR) -pipe -ansi -pedantic -Wall -Wstrict-prototypes -Wmissing-prototypes
PERL=/usr/pkg/bin/perl
INSTALL=install -c 
LDLIBS=-lvers -lopts -lsz
LDFLAGS=-L. -L$(LIBDIR) $(LDLIBS)
ROFF=nroff
TROFF=true
MAN2HTML=true
PREFIX=/home/seebs
USESYS=1
BINDIR="$(PREFIX)/bin/FreeBSD_amd64"
LIBDIR="$(PREFIX)/lib/FreeBSD_amd64"
INCLUDEDIR="$(PREFIX)/include"
MANDIR="$(PREFIX)/man"
PREFIX=/home/seebs
USESYS=1
LDLIBS=-lsz -lvers
LDFLAGS=-L. -L$(PREFIX)/lib $(LDLIBS)
PACKAGE=sz
VERSION=0.9.2

TARGETS=libvers.a VERSION
LIBS=libsz.a
INCLUDES=sz.h

LIB=libsz.a

OBJS=sz.o
MAN3=sz
TEST=sztest
default: all

.SUFFIXES: .1 .3 .cat .ps .html .pl .sh

.3.cat:
	$(ROFF) -man $*.3 > $*.cat

.3.ps:
	$(TROFF) -man $*.3 > $*.ps

.3.html:
	$(MAN2HTML) $*.3 > $*.html

.1.cat:
	$(ROFF) -man $*.1 > $*.cat

.1.ps:
	$(TROFF) -man $*.1 > $*.ps

.1.html:
	$(MAN2HTML) $*.1 > $*.html

.c: $(TARGETS) make.vars $(LIB)
	$(CC) $(CFLAGS) -o $* $*.c $(LDFLAGS)

.sh: Makefile make.vars
	( cat shdefs; sed -e "s/VERSION/$(VERSION)/g" < $*.sh ) > $*
	chmod +x $*

.pl: Makefile make.vars
	( echo "#!$(PERL)"; sed -e "s/VERSION/$(VERSION)/g" < $*.pl ) > $*
	chmod +x $*

clean:
	rm -f *.o $(LIBS) $(UTILS) $(TARGETS) $(TEST)
	if [ -d test ] ; \
	then	( cd test ; $(MAKE) clean ) ; \
	fi

distclean: clean
	rm -f *.ps *.cat *.html *.tar.gz core *.core

dist: distclean
	rm -rf /tmp/$(PACKAGE)-$(VERSION)
	mkdir /tmp/$(PACKAGE)-$(VERSION)
	cp -r -p . /tmp/$(PACKAGE)-$(VERSION)
	( cd /tmp; pax -w -f $(PACKAGE)-$(VERSION).tar $(PACKAGE)-$(VERSION) )
	gzip -f9 /tmp/$(PACKAGE)-$(VERSION).tar
	mv /tmp/$(PACKAGE)-$(VERSION).tar.gz ..
	rm -rf /tmp/$(PACKAGE)-$(VERSION)

install: all
	[ -d $(BINDIR) ] || mkdir -p $(BINDIR)
	[ -d $(INCLUDEDIR) ] || mkdir -p $(INCLUDEDIR)
	[ -d $(LIBDIR) ] || mkdir -p $(LIBDIR)
	[ -d $(MANDIR)/man1 ] || mkdir -p $(MANDIR)/man1
	[ -d $(MANDIR)/man3 ] || mkdir -p $(MANDIR)/man3
	for i in X $(MAN1) ; \
	do	if [ -f $$i.1 ] ; \
		then $(INSTALL) $$i.1 $(MANDIR)/man1; \
		fi \
	done
	for i in X $(MAN3) ; \
	do	if [ -f $$i.3 ] ; \
		then $(INSTALL) $$i.3 $(MANDIR)/man3; \
		fi \
	done
	for i in X $(LIBS) ; \
	do	if [ -f $$i ] ; \
		then $(INSTALL) $$i $(LIBDIR); \
		fi \
	done
	for i in X $(UTILS) ; \
	do	if [ -f $$i ] ; \
		then $(INSTALL) $$i $(BINDIR); \
		fi \
	done
	for i in X $(INCLUDES) ; \
	do	if [ -f $$i ] ; \
		then $(INSTALL) $$i $(INCLUDEDIR); \
		fi \
	done
	for i in X $(DOTFILES) ; \
	do	if [ -f $$i ] ; \
		then $(INSTALL) $$i $(PREFIX)/.$$i; \
		fi \
	done
	$(CATMAN) -M $(MANDIR)

runtest:
	if	[ -d test ] ; \
	then	( cd test; CC=$(CC) $(MAKE) && $(MAKE) clean ) \
	fi

all: $(TARGETS) $(LIBS) $(UTILS) $(TEST) runtest

# We can't remove formatted man pages, because everyone uses different
# conventions.
uninstall:
	for i in $(MAN) ; \
	do	rm -f $(MANDIR)/man3/$$i.3 ; \
	done

VERSION: Makefile
	echo $(VERSION) > VERSION

version.h: Makefile version.h.in
	sed -e "s/VERSION/$(VERSION)/" < version.h.in > $@
	
libvers.a: version.o version.h
	rm -f libvers.a
	ar cr libvers.a version.o

version.o: version.h version.c

docs:
	for i in $(MAN1) ; \
	do	if [ -f $$i.1 ] ; \
		then	$(MAKE) $$i.cat $$i.ps $$i.html ; \
		fi ; \
	done
	for i in $(MAN3) ; \
	do	if [ -f $$i.3 ] ; \
		then	$(MAKE) $$i.cat $$i.ps $$i.html ; \
		fi ; \
	done

$(LIB): $(OBJS)
	rm -f $(LIB)
	ar cr $(LIB) $(OBJS)

makefiles:
	case "X$(ALLDIRS)" in \
	"X")	$(MAKE) makefile ;; \
	*)	AD="$(ALLDIRS)" ; \
		for i in $$AD ; \
		do	( cd $$i ; ./maker ) \
		done \
		;; \
	esac

makefile:
	./maker
