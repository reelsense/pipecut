#ifndef SZ_H
#define SZ_H
#include <stdio.h> /* for size_t */
#include <stddef.h> /* for size_t */

#define SZ_MAGIC 0x01

struct sz;				/* incomplete type */
typedef struct sz sz;			/* opaque reference */

sz	*mem2sz(char *, size_t);	/* makes sz from mem */
sz	*mem2zsz(char *, size_t);	/* makes sz from mem, zen bit set */
sz	*str2sz(char *);		/* makes sz from str */
sz	*str2zsz(char *);		/* makes sz from str, zen bit set */
char	*szencode(void *);		/* produces \ formats */
sz	*str_decode(void *);		/* reads \ formats */

sz	*szfread(FILE *, void *);	/* reads from stream until delim */
int	 szwrite(void *);		/* writes to stdout */
int	 szfwrite(FILE *, void *);	/* writes to stream */
int	 szswrite(char *, size_t, void *);/* writes to string */

sz *	 szgetp(void *);		/* emits sz from sz or str */
void	 szkill(sz *);			/* deletes unused sz's */
void	 szfree(sz *);			/* delete string and substrings */
void	 sztrunc(sz *, size_t);		/* lower length */
sz	*sztail(void *, long);		/* equivalent to t = (s + n) */

sz	*szchr(void *, int);		/* strchr analogue */
char	*szsbrk(void *, void *);	/* strpbrk, returns ptr to data */
char	*szschr(void *, int);		/* strchr, returns ptr to data */
int	 szcmp(void *, void *);		/* strcmp analogue */
size_t	 szcspn(void *, void *);	/* strcspn analogue */
size_t	 szfcspn(void *, int (*)(int));	/* strcspn analogue */
size_t	 szfspn(void *, int (*)(int));	/* strspn analogue */
int	 szicmp(void *, void *);	/* stricmp analogue */
int	 szindex(void *, int);		/* strchr, returns offset or -1 */
size_t	 szlen(void *);			/* strlen analogue */
int	 szncmp(void *, void *, size_t);	/* strncmp analogue */
int	 sznicmp(void *, void *, size_t);	/* strnicmp analogue */
int	 szrindex(void *, int);		/* strrchr, returns offset or -1 */
size_t	 szspn(void *, void *);		/* strspn analogue */
sz	*szcat(void *, void *);		/* strcat analogue */
sz	*szccat(void *, int);		/* adds 1 character to string */
sz	*szcpy(void *, void *);		/* strcpy analogue */
sz	*szdup(void *);			/* strdup analogue */
sz	*szncat(void *, void *, size_t);	/* strncat analogue */
sz	*szncpy(void *, void *, size_t);	/* strncpy analogue */
sz	*szpbrk(void *, void *);		/* strpbrk analogue */
char	*szsrchr(void *, int);		/* strrchr, returns ptr to data */
sz	*szrchr(void *, int);		/* strrchr analogue */
sz	*szsep(sz **, void *);		/* strsep analogue */
sz	*szsz(void *, void *);		/* strstr analogue */
sz	*sztok(void *, void *);		/* strtok analogue */

sz	*szins(sz *, void *, size_t);	/* insert 2nd string in 1st */
sz	*szdel(sz *, size_t, size_t);	/* delete second size bytes at offset */

sz	*sztr(void *, void *, void *);	/* $1=`echo "$1" | tr "$2" "$3"` */

char	*szdata(void *);		/* return data pointer */
int	 szstats(void);			/* print stats to stderr */
sz	*szunzen(sz *);			/* clear zen bit */
sz	*szzen(sz *);			/* set zen bit */

#endif /* SZ_H */
