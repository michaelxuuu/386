#include "inc.h"

// Each path can be seen as parent/name
// * Copy the "name" part into the 'name' buf
// * Optionally copy the "parent" part into the 'parent' buf
// * Return a pointer to the start of the "name" part on success
// * Return 0 when no specified, as in path "////////" or ""
char *getname(char *path, char *name, char *parent) {
    if (!path)
        return 0;
    int len = strlen(path);
    if (!len)
        return 0;
    char *p = path+len-1;
    for (; p!=path && p[0]=='/'; p--);
    if (p==path && len!=1)
        return 0;
    char *pp = p;
    for (; p!=path && p[0]!='/'; p--);
    if (p[0]=='/')
        p++;
    if (pp-p+1>MAXNAME-1)
        return 0;
    strncpy(name, p, pp-p+1);
    name[pp-p+1]=0;
    if (parent) {
        strncpy(parent, path, p-path);
        parent[p-path]=0;
    }
    return p;
}
