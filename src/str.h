#ifndef STR_H
#define STR_H

int str_len(const char *s);
int str_eq(const char *a, const char *b);
int str_has(const char *s, char c);
int str_to_int(const char *s);
void print(int fd, const char *s);

#endif
