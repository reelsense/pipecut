#include "sz.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int
main(void) {
	sz *foo, *bar, *foobar;
	sz *s, *t;
	sz *pwd;
	sz *instest;
	int i;
	char *str = "\\x01This is a test.\\f", *str2;

	foo = str2sz("foo");
	bar = str2sz("bar");
	pwd = str2sz("foo::bar:baz");
	foobar = str2sz("foobar");
	szstats();
	szzen(foobar);
	szunzen(foobar);
	t = str_decode(str);
	str2 = szencode(t);
	szstats();
	if (strcmp(str, str2)) {
		printf("'%s' and '%s' not identical?\n", str, str2);
	}
	szfree(t);
	szstats();
#define PT(x) s = szdup(x); printf(#x ": %s\n", s ? szdata(s) : "<null>"); szfree(s);
#define PI(x) i = x; printf(#x ": %d\n", i);
	PT(szpbrk(foobar, bar));
	PT(szsz(foobar, bar));
	PI(szcspn(foobar, bar));
	PI(szindex(foobar, 'o'));
	PI(szrindex(foobar, 'o'));
	PT(sztail(foobar, 2));
	PT(sztail(foobar, -2));
	szstats();
	szcat(foobar, bar);
	s = foobar;
	printf("barbar: %s\n", s ? szdata(s) : "<null>");
	sztrunc(foobar, 7);
	printf("barb: %s\n", s ? szdata(s) : "<null>");
	sztrunc(foobar, 2);
	printf("ba: %s\n", s ? szdata(s) : "<null>");
	PT(szsz(foobar, foo));
	PT(szpbrk(foobar, "foo"));
	PT(szpbrk(foo, bar));
	PT(szccat(foo, 'd'));
	szstats();
	szfree(foo);
	szfree(bar);
	szfree(foobar);
	szstats();
	printf("%d\n", szindex(pwd, ':'));
	printf("%d\n", szrindex(pwd, ':'));
	printf("%zu\n", szcspn(pwd, ":"));
	for (s = pwd; (t = szsep(&s, ":")) != 0;) {
		printf("pwd: '%s'\n", t ? szdata(t) : "<null>");
	}
	szfree(pwd);
	szstats();
	instest = str2sz("hello, !");
	PT(instest);
	szins(instest, "world", 7);
	PT(instest);
	szdel(instest, 7, 5);
	szins(instest, "there", 7);
	PT(instest);
	t = szsz(instest, "there");
	PT(t);
	szdel(instest, 7, 5);
	PT(t);
	szfree(instest);
	szstats();
	return 0;
}
