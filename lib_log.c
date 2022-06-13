
#include <lib_log.h>

#include <stdarg.h> // va_start, va_end

void lib_dump(const void *p, size_t size, const char *fmt, ...) {
    char *buf;
    size_t len;
    va_list ap;
    int i;

    if(fmt != NULL) {
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        printf("\n");
    }

    printf("   ");
    for(i = 0; i < 16; i++) {
        printf("%02x ", i);
    }
    printf("\n");

    buf = (char *)p;
    len = size;

    while(len) {
        if(((size - len) % 0x10) == 0)
            printf("%s%02x ",
                   ((size - len) == 0) ? "" : "\n",
                   (int)((size - len) & 0xff));

        for(int i = 0; i < 0x10; i++) {
            char val = *buf;

            if(len > 0) {
                printf("%02x ", val & 0xff);
                len--;
                buf++;
            }
        }
    }
    printf("\n");
}

void lib_hexdump(const void *p, size_t size, const char *fmt, ...) {
    char *hbuf, *cbuf;
    size_t hlen, tlen;
    va_list ap;
    int i;

    if(fmt != NULL) {
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        printf("\n");
    }

    printf("   ");
    for(i = 0; i < 16; i++) {
        printf("%02x ", i);
    }
    printf("\n");

    hlen = tlen = size;
    hbuf = cbuf = (char *)p;

    while(tlen) {
        if(((size - tlen) % 0x10) == 0)
            printf("%s%02x ",
                   ((size - tlen) == 0) ? "" : "\n",
                   (int)((size - tlen) & 0xff));

        for(int i = 0; i < 0x10; i++) {
            char val = *hbuf;

            if(hlen > 0) {
                printf("%02x ", val & 0xff);
                hlen--;
                hbuf++;
            }
            else
                printf("   ");
        }
        printf("  ");
        for(int i = 0; i < 0x10; i++) {
            char val = *cbuf;
            if(((size - tlen) % 0x8) == 0)
                printf(" ");

            if(tlen > 0) {
                if(val > 0x20 && val < 0x7e)
                    printf("%c", val);
                else
                    printf(".");
                tlen--;
                cbuf++;
            }
        }
    }
    printf("\n");
}
