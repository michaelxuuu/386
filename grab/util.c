char *strncpy(char *dest, char *src, int n) {
    for (int i = 0; i < n; i++) {
        *dest = *src ? *src : 0;
        dest++;
        if (*src) src++;
    }
    return dest;
}

int strlen(char *s) {
    for (int i = 0;;i++) {
        if (!s[i])
            return i;
    }
}

int strnlen(char *s, int n) {
    for (int i = 0; i < n; i++) {
        if (!s[i])
            return i;
    }
    return n;
}

int strcmp(char *s1, char *s2) {
    for (; *s1 && *s2; s1++, s2++) {
        if (*s1 < *s2)
            return -1;
        else if (*s1 > *s2)
            return 1;
    }
    if (*s1 == *s2) // equally long
        return 0;
    else if (*s1)   // s1 is longer
        return 1;
    else            // s2 is longer
        return -1;
}

void *memset(void *p, int c, int len) {
    char *pp = p;
    for (int i = 0; i < len; i++)
        pp[i] = c;
    return p;
}

void *memcpy(void *dest, void *src, int n) {
    char *d = (char *)dest;
    char *s = (char *)src;
    for (int i = 0; i < n; i++)
        d[i] = s[i];
    return dest;
}