/* sz.c: string library copyright 1995-2001, Peter Seebach.
 * All rights reserved; see the associated files for more info,
 * or write the author.  Search for "seebs" for contact info;
 * any search engine should find me.
 * All rights reserved.
 * All wrongs reversed.
 * This code is distributed without warranty of any sort, implicit
 * or explicit.  You use it at your own risk.
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "sz.h"

static int szmade = 0;
static int szold = 0;

/* ugly lowercase macro - used to make case-insensitive compares
 * nearly-readable */
/* corrected, with some trepidation, after it was pointed out that the
 * test was blatantly wrong on some machines.  The reasoning is that
 * tolower will take any unsigned char, and then we convert the result
 * back to char.  I believe this is expected to behave sensibly on
 * all platforms, although technically a malicious implementor could,
 * I believe, define a stupid conversion from unsigned char to plain char.
 * This doesn't need to handle EOF; it is not intended for use on input
 * streams, only on char objects.
 */
#define L(c) ((char) tolower((unsigned char) (c)))

static size_t mmcspn(char *, void *, size_t);
static size_t mmspn(char *, void *, size_t);

/* support for wider systems */
#define OCTAL_LEN ((CHAR_BIT + 2) / 3)
#define HEX_LEN ((CHAR_BIT + 3) / 4)
#define STR(x) RSTR(x)
#define RSTR(x) #x

enum sz_flags {
	SZ_NONE,
	SZ_ZEN = 0x1,
	SZ_DEAD = 0x2
};

typedef struct szlist szlist;

struct szlist {
	szlist *next;
	sz *s;
};

struct sz {
	unsigned char magic[2];
	int flags;
	int depth; /* used for magic temporaries */
	size_t len;
	size_t rlen;
	size_t offset;
	sz *parent;
	szlist *kids;
	char *data;
};

/* internals */
static void szkid(sz *, sz *);
static void szfixup(sz *);
static sz *szrcpy(sz *, sz *, size_t);
static sz *szdezen(sz *, int);
static unsigned char *expand(sz *, size_t *);

sz *
szgetp(void *v) {
	unsigned char *u = v;
	sz *s;

	if (!v)
		return 0;
	if (*u == (unsigned char) -1) {
		if (u[1] == SZ_MAGIC) {
			s = v;
			++s->depth;
		} else {
			/* magic, but not one of ours. */
			s = 0;
		}
	} else {
		s = str2zsz(v);
	}
	return s;
}

void
szkill(sz *s) {
	if (!s)
		return;
	if (!s->depth)
		szfree(s);
	else
		--s->depth;
}

/* make a zen sz no longer zen */
static sz *
szdezen(sz *s, int morelen) {
	char *tmp;

	if (morelen < 0)
		morelen = 0;

	if (!(s->flags & SZ_ZEN)) {
		return s;
	}

	tmp = malloc(s->len + morelen + 1);
	if (!tmp) {
		return 0;
	}
	memcpy(tmp, s->data, s->len);
	memset(tmp + s->len, '\0', morelen + 1);

	free(s->data);

	s->data = tmp;
	s->flags &= ~SZ_ZEN;
	s->rlen = s->len + morelen + 1;

	return s;
}

/* szfixup attempts to propogate length changes to substrings.
 * It does not affect parents, because it is supposed to be only
 * called on a parent. */
static void
szfixup(sz *s) {
	szlist *k;
	sz *t;

	for (k = s->kids; k; k = k->next) {
		if (k->s) {
			t = k->s;
			/* I don't know how this can happen, but... */
			if (!(t->flags & SZ_ZEN))
				continue;
			if (s->len >= t->offset) {
				if (t->offset + t->len > s->len) {
					t->len = s->len - t->offset;
				}
				t->rlen = s->rlen - t->offset;
				t->data = s->data + t->offset;
				szfixup(t);
			}
		}
	}
}

/* propogate length changes only on strings starting after byte n */
static void
szfixup_n(sz *s, size_t n, int new) {
	szlist *k;
	sz *t;
	size_t end = n + new;

	for (k = s->kids; k; k = k->next) {
		if (k->s) {
			t = k->s;
			/* I don't know how this can happen, but... */
			if (!(t->flags & SZ_ZEN))
				continue;
			if (t->offset >= n) {
				if (new > 0) {
					t->offset += new;
				} else {
					if (t->offset <= end) {
						t->len -= end - t->offset;
						t->offset = n;
					} else {
						t->offset -= new;
					}
				}
			}
			t->len += new;
		}
	}
}

/* add a kid to parent */
static void
szkid(sz *parent, sz *kid) {
	szlist *szl;

	if (!parent || !kid)
		return;

	szl = malloc(sizeof(szlist));
	if (!szl)
		return;
	szl->next = parent->kids;
	szl->s = kid;
	parent->kids = szl;
}

/* remove a kid from parent */
static void
szunkid(sz *parent, sz *kid) {
	szlist *szl, *walk;

	if (!kid || !parent)
		return;

	szl = parent->kids;
	while (szl && szl->s == kid) {
		walk = szl->next;
		szl->next = 0;
		free(szl);
		szl = walk;
	}
	parent->kids = szl;

	if (szl) {
		walk = szl->next;
		while (walk) {
			if (walk->s == kid) {
				szl->next = walk->next;
				free(walk);
				walk = szl->next;
			} else {
				szl = walk;
				walk = walk->next;
			}
		}
	}
}

/* make a new sz, using mem, len, and parent */
static sz *
szznew(char *mem, size_t len, sz *parent) {
	sz *tmp;
	tmp = malloc(sizeof(sz));
	if (!tmp)
		return 0;
	tmp->magic[0] = (unsigned char) -1;
	tmp->magic[1] = SZ_MAGIC;
	tmp->depth = 0;
	tmp->len = len;
	tmp->kids = 0;
	tmp->flags = 0;
	tmp->data = mem;
	tmp->offset = 0;
	tmp->parent = parent;
	tmp->rlen = len;
	tmp->flags = SZ_ZEN;
	++szmade;
	return tmp;
}

/* make a new sz, using parent, if present, and mem, if present, and len */
static sz *
sznew(char *mem, size_t len, sz *parent) {
	sz *tmp;

	tmp = malloc(sizeof(sz));
	if (!tmp)
		return 0;
	tmp->magic[0] = (unsigned char) -1;
	tmp->magic[1] = SZ_MAGIC;
	tmp->len = len;
	tmp->kids = 0;
	tmp->depth = 0;

	if (!parent) {
		tmp->flags = 0;
		tmp->data = malloc(len + 1);
		if (!tmp->data) {
			free(tmp);
			return 0;
		}
		if (mem)
			memcpy(tmp->data, mem, len);
		else
			memset(tmp->data, '\0', len);
		tmp->data[len] = '\0';
		tmp->offset = 0;
		tmp->parent = 0;
		tmp->rlen = len;
	} else {
		tmp->parent = parent;
		szkid(parent, tmp);
		tmp->flags = SZ_ZEN;
		if (mem) {
			tmp->data = mem;
			tmp->offset = mem - (szdata(parent));
			tmp->rlen = parent->rlen - tmp->offset;
		} else {
			/* this is almost certainly wrong, but I think this
			 * case indicates a caller error.  It's our best
			 * bet, I think. */
			tmp->data = parent->data;
			tmp->offset = 0;
			tmp->len = parent->len;
			tmp->rlen = parent->rlen;
		}
	}
	++szmade;
	return tmp;
}

/* housekeeping */
int
szstats(void) {
	fprintf(stderr, "%d new, %d old.\n", szmade, szold);
	if (szmade != szold) {
		return 1;
	} else {
		return 0;
	}
}

/* remove s, and its children */
void
szfree(sz *s) {
	szlist *szl, *tmp = 0;

	if (!s)
		return;

	/* this monstrosity is for use with a debugging malloc wrapper, which
	 * sets all one bits in freed memory.  It allows execution to fail
	 * more dramatically later, rather than with a core dump right away.
	 */
	if ((unsigned long) s->data == ~0UL) {
		return;
	}
	if (s->depth) {
		fprintf(stderr, "sz: warning: freeing sz, depth of %d, text %s\n",
			s->depth, s->data);
	}
	for (szl = s->kids; szl; szl = szl->next) {
		if (tmp)
			free(tmp);
		tmp = szl;
		szl->s->parent = 0;
		szfree(szl->s);
	}
	if (tmp)
		free(tmp);
	if (!(s->flags & (SZ_ZEN | SZ_DEAD)) && s->data) {
		free(s->data);
	}
	if (s->parent) {
		szunkid(s->parent, s);
	}
	s->flags |= SZ_DEAD;
	++szold;

	free(s);
}

/* These four functions are all fairly trivial. */
sz *
str2zsz(char *s) {
	return szznew(s, strlen(s), 0);
}

sz *
str2sz(char *s) {
	return sznew(s, strlen(s), 0);
}

sz *
mem2zsz(char *s, size_t len) {
	return szznew(s, len, 0);
}

sz *
mem2sz(char *s, size_t len) {
	return sznew(s, len, 0);
}

/* this attempts to produce a string representation of an sz */
/* arguably, it's cruft to calcuate the length, then allocate, but it's
 * also cruft to reallocate repeatedly. */
char *
szencode(void *v) {
	size_t i, len = 0;
	char *t;
	sz *s = szgetp(v);

	if (!s)
		return NULL;

	for (i = 0; i < s->len; ++i) {
		if (s->data[i] == '\\') {
			len += 2;
		} else if (isprint(s->data[i])) {
			++len;
		} else {
			switch (s->data[i]) {
			case '\a':
			case '\b':
			case '\f':
			case '\n':
			case '\r':
			case '\t':
				len += 2;
				break;
			default:
				len += HEX_LEN + 2; /* '\xHH' */
				break;
			}
		}
	}
	++len; /* for terminating nul */

	t = malloc(len);
	if (!t)
		return NULL;
	*t = '\0';

	/* the constant modification of t here is to make strcat faster.
	 * The little tin god is thus appeased. */
	for (i = 0; i < s->len; ++i) {
		if (s->data[i] == '\\') {
			*t++ = '\\';
			*t++ = '\\';
			*t = '\0';
		} else if (isprint(s->data[i])) {
			*t++ = s->data[i];
			*t = '\0';
		} else {
			switch (s->data[i]) {
			case '\a':
				strcat(t, "\\a");
				t += 2;
				break;
			case '\b':
				strcat(t, "\\b");
				t += 2;
				break;
			case '\f':
				strcat(t, "\\f");
				t += 2;
				break;
			case '\n':
				strcat(t, "\\n");
				t += 2;
				break;
			case '\r':
				strcat(t, "\\r");
				t += 2;
				break;
			case '\t':
				strcat(t, "\\t");
				t += 2;
				break;
			default:
				sprintf(t, "\\x%0*X", HEX_LEN,
					(unsigned char) s->data[i]);
				t += 2 + HEX_LEN;
				break;
			}
		}
	}

	*t = '\0';
	t -= (len - 1); /* we backtrack to the beginning ... */
	szkill(s);
	return t;
}

/* given a string looking like "\a" (actual backslash a, not containing a
 * 0x07 in ASCII land), produce an sz containing whatever '\a' would have
 * been. */
sz *
str_decode(void *v) {
	size_t i, j;
	char cbuf[OCTAL_LEN + 1];
	size_t len = 0;
	sz *u = szgetp(v);
	sz *tmp;
	char *s = szdata(u);
	char *t;

	if (!s)
		return NULL;

	for (i = 0; i < u->len; ++i) {
		s = u->data + i;
		if (*s == '\\') {
			switch (s[1]) {
			case 'a':
			case 'b':
			case 'f':
			case 'n':
			case 'r':
			case 't':
			case '\\':
				++len;
				++i;
				break;
			case 'x':
				++len;
				i += HEX_LEN + 1;
				break;
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
				++len;
				j = strspn(s + 1, "01234567");
				len = (j > OCTAL_LEN) ? OCTAL_LEN : j;
				i += len;
				break;
			default:
				errno = EDOM;
				szkill(u);
				return NULL;
				break;
			}
		} else {
			++len;
		}
	}

	s = szdata(u);

	tmp = sznew(0, len, 0);
	if (!tmp)
		return NULL;

	t = szdata(tmp);
	for (i = 0; i < u->len; ++i) {
		s = u->data + i;
		if (*s == '\\') {
			switch (s[1]) {
			case 'a':
				*t++ = '\a';
				++i;
				break;
			case 'b':
				*t++ = '\b';
				++i;
				break;
			case 'f':
				*t++ = '\f';
				++i;
				break;
			case 'n':
				*t++ = '\n';
				++i;
				break;
			case 'r':
				*t++ = '\r';
				++i;
				break;
			case 't':
				*t++ = '\t';
				++i;
				break;
			case '\\':
				*t++ = '\\';
				++i;
				break;
			case 'x':
				strncpy(cbuf, s + 2, HEX_LEN);
				cbuf[HEX_LEN] = '\0';
				*t++ = (unsigned char) strtol(cbuf, 0, 16);
				i += HEX_LEN + 1;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				j = strspn(s + 1, "01234567");
				len = (j > OCTAL_LEN) ? OCTAL_LEN : j;
				strncpy(cbuf, s + 1, len);
				cbuf[len] = '\0';
				*t++ = (unsigned char) strtol(cbuf, 0, 8);
				i += len;
				break;
			default:
				errno = EDOM;
				return NULL;
				break;
			}
		} else {
			*t++ = *s;
		}
	}
	*t = '\0';
	szkill(u);
	return tmp;
}

int
sznicmp(void *v1, void *v2, size_t len) {
	size_t i = 0;
	int ret = 0;
	int mlen = len;
	sz *s1 = szgetp(v1), *s2 = szgetp(v2);

	if (mlen > s1->len)
		mlen = s1->len;
	if (mlen > s2->len)
		mlen = s2->len;

	while (i < mlen) {
		if (L(s1->data[i]) < L(s2->data[i])) {
			ret = -1; break;
		} else if (L(s1->data[i]) > L(s2->data[i])) {
			ret = 1; break;
		}
		++i;
	}
	if (!ret && i < len) {
		if (s1->len < s2->len)
			ret = -1;
		else if (s1->len > s2->len)
			ret = 1;
	}
	szkill(s1);
	szkill(s2);
	return ret;
}

int
szncmp(void *v1, void *v2, size_t len) {
	size_t i = 0;
	int ret = 0;
	int mlen = len;
	sz *s1 = szgetp(v1), *s2 = szgetp(v2);

	if (mlen > s1->len)
		mlen = s1->len;
	if (mlen > s2->len)
		mlen = s2->len;

	while (i < mlen) {
		if (s1->data[i] < s2->data[i]) {
			ret = -1; break;
		} else if (s1->data[i] > s2->data[i]) {
			ret = 1; break;
		}
		++i;
	}
	if (!ret && i < len) {
		if (s1->len < s2->len)
			ret = -1;
		else if (s1->len > s2->len)
			ret = 1;
	}
	szkill(s1);
	szkill(s2);
	return ret;
}

int
szicmp(void *v1, void *v2) {
	size_t i = 0;
	int ret = 0;
	sz *s1 = szgetp(v1), *s2 = szgetp(v2);

	while (i < s1->len && i < s2->len) {
		if (L(s1->data[i]) < L(s2->data[i])) {
			ret = -1; break;
		} else if (L(s1->data[i]) > L(s2->data[i])) {
			ret = 1; break;
		}
		++i;
	}
	if (!ret) {
		if (s1->len < s2->len)
			ret = -1;
		else if (s1->len > s2->len)
			ret = 1;
	}
	szkill(s1);
	szkill(s2);
	return ret;
}

int
szcmp(void *v1, void *v2) {
	size_t i = 0;
	int ret = 0;
	sz *s1 = szgetp(v1), *s2 = szgetp(v2);

	while (i < s1->len && i < s2->len) {
		if (s1->data[i] < s2->data[i]) {
			ret = -1; break;
		} else if (s1->data[i] > s2->data[i]) {
			ret = 1; break;
		}
		++i;
	}
	if (!ret) {
		if (s1->len < s2->len)
			ret = -1;
		else if (s1->len > s2->len)
			ret = 1;
	}
	szkill(s1);
	szkill(s2);
	return ret;
}

/* although index() and rindex() didn't do this on systems that used to have
 * them, they shoulda; I use them this way for idiom's sake, and because
 * szchr() is a memory leak */
int
szindex(void *v, int u) {
	char *cp;
	sz *s = szgetp(v);

	if (!s)
		return -1;

	cp = memchr(s->data, u, s->len);

	if (cp) {
		int ret = cp - s->data;
		szkill(s);
		return ret;
	} else {
		szkill(s);
		return -1;
	}
}

int
szrindex(void *v, int u) {
	char *cp, *ocp = 0;
	size_t ct = 0;
	sz *s = szgetp(v);

	cp = szdata(s);
	while ((cp = memchr(cp, u, s->len - ct)) != 0) {
		ocp = cp++;
		ct = cp - s->data;
	}

	if (ocp) {
		int ret = ocp - s->data;
		szkill(s);
		return ret;
	} else {
		szkill(s);
		return -1;
	}
}

/* mostly used to avoid a memory leak */
char *
szschr(void *v, int c) {
	sz *s = szgetp(v);
	char *t;

	if (!s || !s->data)
		return 0;
	t = memchr(s->data, c, s->len);
	szkill(s);
	return t;
}

sz *
szchr(void *v, int c) {
	char *cp;
	sz *s = szgetp(v);

	if (!s || !s->data)
		return 0;

	cp = memchr(s->data, c, s->len);

	if (cp) {
		/* agh!  a memory leak if you just test it.  no good fix. */
		sz *tmp = sznew(cp, s->len - (cp - s->data), s);
		/* More subtle:  We can't kill s since we still want to
		 * refer to it.
		 */
		if (s->depth)
			--s->depth;
		return tmp;
	} else {
		szkill(s);
		return 0;
	}
}

char *
szsrchr(void *v, int c) {
	char *cp, *ocp = 0;
	int ct = 0;
	sz *s = szgetp(v);

	if (!s || !s->data)
		return 0;

	cp = s->data;
	while ((cp = memchr(cp, c, s->len - ct)) != 0) {
		ocp = cp++;
		ct = cp - s->data;
	}
	szkill(s);
	return ocp;
}

sz *
szrchr(void *v, int c) {
	char *cp, *ocp = 0;
	int ct = 0;
	sz *s = szgetp(v);
	sz *ret;

	if (!s || !s->data)
		return 0;

	cp = (char *) s->data;
	while ((cp = memchr(cp, c, s->len - ct)) != 0) {
		ocp = cp++;
		ct = cp - s->data;
	}

	if (ocp) {
		/* a memory leak, but if you're using rchr, it's probably
		 * for purposes of using the resulting pointer, so not so
		 * bad. */
		if (s->depth)
			--s->depth;
		ret = sznew(ocp, s->len - (ocp - s->data), s);
	} else {
		/* This is in here because we don't kill it when returning
		 * a reference */
		szkill(s);
		ret = 0;
	}

	return ret;
}

sz *
szncat(void *v1, void *v2, size_t len) {
	char *tmp;
	size_t mlen;
	sz *s1 = szgetp(v1);
	sz *s2 = szgetp(v2);

	if (!s1)
		return 0;

	if (!s2) {
		return s1;
	}

	if (s1->depth)
		--s1->depth;
	if (s1->parent) {
		szncat(s1->parent, s2, len);
		szkill(s2);
		return s1;
	}

	if (len > s1->len + s2->len)
		mlen = s1->len + s2->len;
	else
		mlen = len;

	if (!szdezen(s1, len - s1->len)) {
		szkill(s1);
		szkill(s2);
		return 0;
	}

	if (len > s1->rlen) {
		tmp = realloc(s1->data, len + 1);
		if (!tmp)
			return NULL;
		s1->rlen = len;
		s1->data = tmp;
	}

	memcpy(s1->data + s1->len, s2->data, mlen - s1->len);
	memset(s1->data + mlen, 0, len + 1 - mlen);
	s1->len = len;

	szkill(s2);
	return s1;
}

sz *
szccat(void *v, int c) {
	char *tmp;
	sz *s = szgetp(v);

	if (s->depth)
		--s->depth;
	if (s->parent) {
		szccat(s->parent, c);
		return s;
	}

	if (!szdezen(s, 1)) {
		return s;
	}

	if (s->len + 1 >= s->rlen) {
		tmp = realloc(s->data, s->len + 2);
		if (!tmp) {
			return 0;
		}
		s->data = tmp;
		s->rlen = s->len + 2;
	}

	s->data[s->len++] = (unsigned char) c;
	s->data[s->len] = 0;

	return s;
}

sz *
szcat(void *v1, void *v2) {
	char *tmp;
	sz *s1 = szgetp(v1);
	sz *s2 = szgetp(v2);

	if (!s1) {
		szkill(s2);
		return 0;
	}
	if (s1->depth)
		--s1->depth;
	if (!s2)
		return s1;

	if (s1->parent) {
		szcat(s1->parent, s2);
		szkill(s2);
		return s1;
	}

	if (!szdezen(s1, s2->len)) {
		szkill(s2);
		return s1;
	}

	if (s1->len + s2->len > s1->rlen) {
		tmp = realloc(s1->data, s1->len + s2->len + 1);
		s1->rlen = s1->len + s2->len;
		if (!tmp)
			return NULL;
		s1->data = tmp;
	}

	memcpy(s1->data + s1->len, s2->data, s2->len);
	s1->len += s2->len;
	s1->data[s1->len] = '\0';

	szfixup(s1);
	szkill(s2);
	return s1;
}

/* this does the internal guts of copying onto a string correctly... it's
 * not as easy as it looks. */
static sz *
szrcpy(sz *dest, sz *src, size_t offset) {
	char *tmp;

	if (dest->parent) {
		szrcpy(dest->parent, src, offset + dest->offset);
		return dest;
	}

	if (!szdezen(dest, src->len - (dest->len - offset))) {
		return dest;
	}

	if (dest->len >= src->len + offset) {
		dest->len = src->len + offset;
		memcpy(dest->data + offset, src->data, src->len);
	} else {
		tmp = realloc(dest->data, src->len + offset + 1);
		if (!tmp)
			return NULL;
		dest->data = tmp;
		dest->rlen = src->len + offset;
		memcpy(dest->data + offset, src->data, src->len);
	}
	dest->data[src->len + offset] = '\0';
	szfixup(dest);

	return dest;
}

sz *
szcpy(void *v1, void *v2) {
	sz *s1 = szgetp(v1);
	sz *s2 = szgetp(v2);

	if (s1->depth)
		--s1->depth;
	szrcpy(s1, s2, 0);
	szkill(s2);
	return s1;
}

size_t
szlen(void *v) {
	sz *s = szgetp(v);
	size_t ret;

	if (s)
		ret = s->len;
	else
		ret = 0;

	szkill(s);
	return ret;
}

size_t
szfcspn(void *v1, int (*f)(int)) {
	size_t ct = 0;
	sz *s1 = szgetp(v1);

	if (!s1) {
		return 0;
	}

	if (!f) {
		szkill(s1);
		return s1->len;
	}

	while (ct < s1->len && !f(s1->data[ct]))
		++ct;

	szkill(s1);
	return ct;
}

size_t
szcspn(void *v1, void *v2) {
	size_t ct = 0;
	sz *s1 = szgetp(v1);
	sz *s2 = szgetp(v2);

	if (!s1) {
		szkill(s2);
		return 0;
	}

	if (!s2) {
		szkill(s1);
		return s1->len;
	}

	while (ct < s1->len && (szindex(s2, s1->data[ct]) == -1))
		++ct;

	szkill(s1);
	szkill(s2);
	return ct;
}

size_t
szfspn(void *v1, int (*f)(int)) {
	int ct = 0;
	sz *s1 = szgetp(v1);

	if (!s1)
		return 0;

	if (!f)
		return 0;

	while (ct < s1->len && f(s1->data[ct]))
		++ct;

	szkill(s1);
	return ct;
}

size_t
szspn(void *v1, void *v2) {
	int ct = 0;
	sz *s1 = szgetp(v1);
	sz *s2 = szgetp(v2);

	if (!s1) {
		szkill(s2);
		return 0;
	}

	if (!s2) {
		szkill(s1);
		return 0;
	}

	while (ct < s1->len && (szindex(s2, s1->data[ct]) != -1))
		++ct;

	szkill(s1);
	szkill(s2);
	return ct;
}

sz *
szdup(void *v) {
	sz *tmp;
	sz *s = szgetp(v);

	if (!s)
		return NULL;
	if ((s->flags & SZ_ZEN) && !s->parent) {
		tmp = mem2zsz(s->data, s->len);
	} else {
		tmp = sznew(s->data, s->len, 0);
	}
	szkill(s);
	return tmp;
}

sz *
szncpy(void *v1, void *v2, size_t len) {
	size_t mlen;
	sz *s1 = szgetp(v1);
	sz *s2 = szgetp(v2);

	if (!s1) {
		szkill(s2);
		return 0;
	}
	if (!s2) {
		szkill(s1);
		return s1;
	}
	if (s1->depth)
		--s1->depth;

	if (len > s1->rlen) {
		char *tmp = realloc(s1->data, len + 1);
		if (!tmp)
			return NULL;
		s1->data = tmp;
		s1->rlen = len;
	}

	if (len > s2->len)
		mlen = s2->len;
	else
		mlen = len;
	memcpy(s1->data, s2->data, mlen);
	memset(s1->data + mlen, 0, len + 1 - mlen);

	s1->len = len;

	szkill(s2);
	return s1;
}

/* despite the fact that strtok() sucks, we implement it for compatability. */
sz *
sztok(void *v, void *d) {
	static sz *internal;
	static size_t pos;
	int len;
	sz *src = szgetp(v);
	sz *tmp;
	sz *delim = szgetp(d);

	if (src) {
		internal = src;
		pos = 0;
		if (src->depth)
			--src->depth;
	}

	if (pos >= internal->len) {
		return 0;
	}

	len = mmcspn(internal->data + pos, delim, internal->len - pos);

	sztrunc(internal, pos + len);
	tmp = sznew(internal->data + pos, len, internal);

	pos += len;
	pos += mmspn(internal->data + pos, delim, internal->len - pos) + 1;

	szkill(delim);
	return tmp;
}

char *
szsbrk(void *v1, void *v2) {
	size_t len;
	sz *s1 = szgetp(v1);
	sz *s2 = szgetp(v2);

	if (!s1 || !s1->data) {
		szkill(s2);
		return 0;
	}

	if (!s2) {
		szkill(s1);
		return s1->data;
	}

	if (s1->depth)
		--s1->depth;

	len = szcspn(s1, s2);

	if (len >= s1->len) {
		szkill(s1);
		szkill(s2);
		return 0;
	} else {
		char *ret = s1->data + len;
		szkill(s1);
		szkill(s2);
		return ret;
	}
}

sz *
szpbrk(void *v1, void *v2) {
	size_t len;
	sz *ret;
	sz *s1 = szgetp(v1);
	sz *s2 = szgetp(v2);

	len = szcspn(s1, s2);

	if (len >= s1->len) {
		ret = 0;
	} else {
		ret = sztail(s1, (long) len);
	}

	if (s1->depth)
		--s1->depth;
	szkill(s2);
	return ret;
}

sz *
sztail(void *v, long n) {
	size_t len;
	sz *s1 = szgetp(v);
	sz *s2;

	if (n < 0) {
		n *= -1;
		if (n > s1->len)
			n = 0;
		else
			n = s1->len - n;
	}
	if (n > s1->len)
		len = 0;
	else
		len = s1->len - n;

	s2 = sznew(s1->data + (s1->len - len), len, s1);

	if (s1->depth)
		--s1->depth;
	return s2;
}

char *
szdata(void *v) {
	sz *s = szgetp(v);
	char *data;

	if (!s)
		return 0;

	data = s->data;
	szkill(s);
	return data;
}

/* expands 'a-z' into 'abcdefghijklmnopqrstuvwxyz' in ASCII-land; I don't
 * really want to think about what it does in EBCDIC. */
static unsigned char *
expand(sz *from, size_t *lenp) {
	unsigned char *buf = 0, *t, *tmp;
	size_t i;
	unsigned char j;
	int s;
	size_t size = 16;
	size_t pos = 0;

	if (!from || !from->data || !from->len)
		return 0;
	t = (unsigned char *) from->data;

	buf = malloc(size);
	if (!buf)
		return 0;
	buf[pos++] = t[0];
	for (i = 1; i < (from->len - 1); ++i) {
		if (t[i] == '-') {
			if (t[i - 1] < t[i + 1]) {
				s = 1;
			} else {
				s = -1;
			}
			/* ack. */
			for (j = t[i - 1] + s;
			     s > 0 ? (j <= t[i + 1]) : (j >= t[i + 1]);
			     j += s) {
				if (pos + 1 >= size) {
					tmp = realloc(buf, size *= 2);
					if (tmp) {
						buf = tmp;
					} else {
						free(buf);
						return 0;
					}
				}
				buf[pos++] = j;
			}
			++i; /* skip the 2nd letter of a-z */
		} else {
			if (pos + 1 >= size) {
				tmp = realloc(buf, size *= 2);
				if (tmp) {
					buf = tmp;
				} else {
					free(buf);
					return 0;
				}
			}
			buf[pos++] = t[i];
		}
	}
	/* if we haven't finished the source off, we have one char left.  We
	 * might have finished it off if the 2nd to last character were a
	 * hyphen, so the last char was used as part of the range.
	 */
	if (i == (from->len - 1)) {
		if (pos + 1 >= size) {
			tmp = realloc(buf, size *= 2);
			if (tmp) {
				buf = tmp;
			} else {
				free(buf);
				return 0;
			}
		}
		buf[pos++] = t[i];
	}

	if (lenp)
		*lenp = pos;
	return (unsigned char *) buf;
}

/* imitate 'tr'.  We use unsigned chars because they have more consistent
 * semantics */
sz *
sztr(void *v, void *vfrom, void *vto) {
	unsigned char *t, *tbuf, *fbuf;
	size_t i, flen, tlen;
	sz *s = szgetp(v);
	sz *from = szgetp(vfrom), *to = szgetp(vto);

	if (!s || !from || !to) {
		szkill(s);
		szkill(from);
		szkill(to);
		return 0;
	}

	fbuf = expand(from, &flen);
	if (!fbuf) {
		fbuf = (unsigned char *) from->data;
		flen = from->len;
	}

	tbuf = expand(to, &tlen);
	if (!tbuf) {
		tbuf = (unsigned char *) to->data;
		tlen = to->len;
	}

	for (i = 0; i < s->len; ++i) {
		t = memchr(fbuf, s->data[i], flen);
		if (t && (t - fbuf) <= tlen) {
			s->data[i] = tbuf[t - fbuf];
		}
	}

	if (fbuf != (unsigned char *) from->data)
		free(fbuf);
	if (tbuf != (unsigned char *) to->data)
		free(tbuf);
	szkill(from);
	szkill(to);
	if (s->depth)
		--s->depth;
	return s;
}

static size_t
mmspn(char *mem, void *v, size_t len) {
	size_t ct = 0;
	sz *s = szgetp(v);

	while (ct < len && (szindex(s, mem[ct]) != -1))
		++ct;

	szkill(s);
	return ct;
}

static size_t
mmcspn(char *mem, void *v, size_t len) {
	size_t ct = 0;
	sz *s = szgetp(v);

	while (ct < len && (szindex(s, mem[ct]) == -1))
		++ct;

	szkill(s);
	return ct;
}

sz *
szsep(sz **szp, void *d) {
	sz *orig;
	sz *new;
	size_t len;
	sz *delim;

	if (!szp || !*szp) {
		return 0;
	}
	delim = szgetp(d);

	orig = *szp;

	len = szcspn(orig, delim);

	if (len >= orig->len)
		new = 0;
	else {
		new = sztail(orig, len + 1);
		sztrunc(orig, len);
	}
	*szp = new;

	szkill(delim);
	return orig;
}

void
sztrunc(sz *s, size_t len) {
	if (s->parent) {
		sztrunc(s->parent, len + s->offset);
		return;
	}

	if (len > s->rlen)
		len = s->rlen;

	if (len == s->len)
		return;

	if (szdezen(s, 0)) {
		s->len = len;
		s->data[s->len] = '\0';
		szfixup(s);
	}
}

sz *
szsz(void *v1, void *v2) {
	int c;
	size_t i, j;
	sz *s1 = szgetp(v1);
	sz *s2 = szgetp(v2);

	if (!s1 || !s2) {
		szkill(s2);
		return 0;
	}
	c = s2->data[0];
	for (i = 0; i < s1->len; ++i) {
		if (s1->data[i] == c) {
			for (j = 1; j < (s1->len - i) && j < s2->len; ++j) {
				if (s1->data[j + i] != s2->data[j])
					break;
			}
			if (j == s2->len) {
				if (s1->depth)
					--s1->depth;
				szkill(s2);
				return sztail(s1, i);
			}
		}
	}
	szkill(s1);
	szkill(s2);
	return 0;
}

sz *
szunzen(sz *s) {
	if (!s)
		return 0;
	s->flags &= ~SZ_ZEN;
	return s;
}

sz *
szzen(sz *s) {
	if (!s)
		return 0;
	s->flags |= SZ_ZEN;
	return s;
}

int
szswrite(char *into, size_t max, void *v) {
	sz *s = szgetp(v);
	size_t len = max;

	if (!s || !s->data)
		return 0;

	if (s->len < len)
		len = s->len;

	memcpy(into, s->data, len);
	szkill(s);
	return len;
}

int
szfwrite(FILE *fp, void *v) {
	sz *s = szgetp(v);
	int ret;

	if (!s || !s->data)
		return EOF;

	ret = fwrite(s->data, 1, s->len, fp);
	szkill(s);
	return ret;
}

int
szwrite(void *v) {
	return szfwrite(stdout, v);
}

sz *
szfread(FILE *f, void *v) {
	sz *delim, *new;
	char *buf = 0;
	size_t size = 0;
	size_t used = 0;
	int c;

	if (!f)
		return 0;

	c = fgetc(f);
	if (c == EOF) {
		return 0;
	}
	ungetc(c, f);

	size = 16;
	buf = malloc(16);
	if (!buf)
		return 0;

	delim = szgetp(v);

	if (!delim) {
		int ret;
		char *tmp;
		do {
			ret = fread(buf + used, 1, size - used, f);
			if (ret == size - used) {
				used += ret;
				tmp = realloc(buf, size *= 2);
				if (!tmp) { 
					free(buf);
					return 0;
				}
				buf = tmp;
			} else if (ret >= 0) {
				used += ret;
			}
		} while (!feof(f));
		new = sznew(buf, used, 0);
	} else {
		int c;
		new = str2sz("");
		if (!new)
			return 0;

		while ((c = fgetc(f)) != EOF) {
			if (szschr(delim, c)) {
				break;
			} else {
				szccat(new, c);
			}
		}
	}
	return new;
}

sz *
szins(sz *dest, void *v, size_t offset) {
	sz *src = szgetp(v);
	char *t;
	if (!dest || !src || offset > dest->len)
		return 0;

	if (dest->parent) {
		szins(dest->parent, src, offset + dest->offset);
		szkill(src);
		return dest;
	}

	t = malloc(dest->len + src->len + 1);
	memcpy(t, dest->data, offset);
	memcpy(t + offset, src->data, src->len);
	memcpy(t + offset + src->len, dest->data + offset, dest->len - offset);
	if (dest->flags & SZ_ZEN) {
		dest->flags &= ~SZ_ZEN;
	} else {
		free(dest->data);
	}
	dest->data = t;
	dest->len = dest->len + src->len;
	dest->data[dest->len] = '\0';

	szfixup_n(dest, offset, src->len);
	szkill(src);
	return dest;
}

sz *
szdel(sz *dest, size_t offset, size_t len) {
	if (!dest || (len + offset) > dest->len)
		return 0;

	if (dest->parent) {
		szdel(dest->parent, offset + dest->offset, len);
		return dest;
	}
	memmove(dest->data + offset, dest->data + offset + len,
		dest->len - (offset + len));
	dest->len -= len;
	dest->data[dest->len] = '\0';

	szfixup_n(dest, offset, -len);
	return dest;
}
