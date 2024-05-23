#define assert(expr)\
    do\
    {\
        if (!(expr)) {\
            printf("assertion failed at %d in %s", __LINE__, __FILE__);\
            for (;;);\
        }\
    } while (0)
