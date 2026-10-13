/* small string helpers, since we don't have libc */
#include "sys.h"
#include "str.h"

int str_len(const char *s)
{
	int n = 0;

	while (s[n])
		n++;
	return n;
}

int str_eq(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return *a == *b;
}

int str_has(const char *s, char c)
{
	while (*s) {
		if (*s == c)
			return 1;
		s++;
	}
	return 0;
}

int str_to_int(const char *s)
{
	int n = 0;

	while (*s >= '0' && *s <= '9') {
		n = n * 10 + (*s - '0');
		s++;
	}
	return n;
}

void print(int fd, const char *s)
{
	sys_write(fd, s, str_len(s));
}

/*
 * gcc is allowed to emit calls to these two even if we never call
 * them ourselves, so they have to exist somewhere.
 */
void *memset(void *dst, int c, unsigned long n)
{
	unsigned char *d = dst;

	while (n--)
		*d++ = c;
	return dst;
}

void *memcpy(void *dst, const void *src, unsigned long n)
{
	unsigned char *d = dst;
	const unsigned char *s = src;

	while (n--)
		*d++ = *s++;
	return dst;
}
