#ifndef _util_h_
#define _util_h_

void *memcpy(void *dest, void *src, int n);
void *memset(void *p, int c, int len);
int strcmp(char *s1, char *s2);
int strnlen(char *s, int n) ;
int strlen(char *s);
char *strncpy(char *dest, char *src, int n);

#endif