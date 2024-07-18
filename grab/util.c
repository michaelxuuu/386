#include <types.h>
#include <printf.h>
#include <assert.h>

char *strncpy(char *dest, char *src, int n)
{
        for (int i = 0; i < n; i++) {
                *dest = *src ? *src : 0;
                dest++;
                if (*src) src++;
        }
        return dest;
}

int strlen(char *s)
{
        for (int i = 0;; i++) {
                if (!s[i]) return i;
        }
}

int strnlen(char *s, int n)
{
        for (int i = 0; i < n; i++) {
                if (!s[i]) return i;
        }
        return n;
}

int strcmp(char *s1, char *s2)
{
        for (; *s1 && *s2; s1++, s2++) {
                if (*s1 < *s2)
                        return -1;
                else if (*s1 > *s2)
                        return 1;
        }
        if (*s1 == *s2) // equally long
                return 0;
        else if (*s1) // s1 is longer
                return 1;
        else // s2 is longer
                return -1;
}

int strncmp(char *s1, char *s2, int n)
{
        int i = 0;
        for (; i < n && *s1 && *s2; s1++, s2++) {
                if (*s1 < *s2)
                        return -1;
                else if (*s1 > *s2)
                        return 1;
        }
        // Reached the end without any difference found
        if (i == n) return 0;
        if (*s1 == *s2) // equally long
                return 0;
        else if (*s1) // s1 is longer
                return 1;
        else // s2 is longer
                return -1;
}

void strcpy(char *dest, char *src)
{
        for (; *src; src++, dest++)
                *dest = *src;
        *dest = 0;
}

void *memset(void *p, int c, int len)
{
        char *pp = p;
        for (int i = 0; i < len; i++)
                pp[i] = c;
        return p;
}

void *memcpy(void *dest, void *src, int n)
{
        char *d = (char *)dest;
        char *s = (char *)src;
        for (int i = 0; i < n; i++)
                d[i] = s[i];
        return dest;
}

int isdigit(char c) { return '0' <= c && c <= '9'; }

int isalpha(char c) { return 'a' <= c && c <= 'z'; }

uint32_t str2uint(char *s)
{
        assert(s);
        int base = 1; // 1, 10, 100, ...
        int ndigits = 0;
        // Allow 10 digits max
        for (int i = 1; s[i] && i < 10; i++, ndigits++)
                base *= 10;
        // Must not overflow
        // 1. Must be less than 10 digits
        assert(ndigits <= 10);
        // 2. Must smaller than max uint
        if (++ndigits == 10) assert(strncmp("4294967295", s, 10) >= 0);
        uint32_t x = 0;
        for (int i = 0; i < ndigits; i++, base /= 10)
                x += (s[i] - '0') * base;
        return x;
}

int isprint(int c) { return ((c) >= ' ') && ((c) <= 126); }

int isspace(int c) { return c == '\t' || c == ' '; }
