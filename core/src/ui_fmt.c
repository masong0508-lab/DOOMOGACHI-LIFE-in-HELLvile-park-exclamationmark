/* ui_fmt.c - a tiny, freestanding printf (owner: foundation).
 * Supports %d %i %u %x %X %s %c %% with flags '-' (left align) and '0'
 * (zero pad) and a decimal width ("%3d", "%02d", "%-8s"). Always
 * NUL-terminates when cap > 0; returns the length it would have written
 * (like snprintf), excluding the NUL. */
#include <stdarg.h>
#include "doomo/ui.h"

typedef struct { char *buf; int cap, len; } Out;

static void put(Out *o, char c)
{
    if (o->len < o->cap - 1) o->buf[o->len] = c;
    o->len++;
}

static void put_padded(Out *o, const char *s, int n, int width, int left, char pad,
                       int sign)
{
    int fill = width - n - (sign ? 1 : 0), i;
    if (!left && pad == ' ') while (fill-- > 0) put(o, ' ');
    if (sign) put(o, (char)sign);
    if (!left && pad == '0') while (fill-- > 0) put(o, '0');
    for (i = 0; i < n; i++) put(o, s[i]);
    if (left) while (fill-- > 0) put(o, ' ');
}

int doomo_vsnprintf(char *buf, int cap, const char *fmt, va_list ap)
{
    Out o;
    o.buf = buf; o.cap = cap; o.len = 0;
    if (!fmt) fmt = "";
    while (*fmt) {
        char c = *fmt++;
        int left = 0, width = 0;
        char pad = ' ';
        if (c != '%') { put(&o, c); continue; }
        for (;;) {
            if (*fmt == '-') { left = 1; fmt++; }
            else if (*fmt == '0') { pad = '0'; fmt++; }
            else break;
        }
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        if (*fmt == 'l') fmt++;           /* %ld == %d (int is 32-bit) */
        c = *fmt;
        if (!c) break;
        fmt++;
        switch (c) {
        case 'd': case 'i': {
            int v = va_arg(ap, int);
            unsigned int u = v < 0 ? 0u - (unsigned int)v : (unsigned int)v;
            char tmp[12];
            int n = 0;
            do { tmp[11 - n] = (char)('0' + u % 10); u /= 10; n++; } while (u);
            put_padded(&o, tmp + 12 - n, n, width, left, pad, v < 0 ? '-' : 0);
            break; }
        case 'u': case 'x': case 'X': {
            unsigned int u = va_arg(ap, unsigned int);
            unsigned int base = c == 'u' ? 10u : 16u;
            const char *dig = c == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
            char tmp[12];
            int n = 0;
            do { tmp[11 - n] = dig[u % base]; u /= base; n++; } while (u);
            put_padded(&o, tmp + 12 - n, n, width, left, pad, 0);
            break; }
        case 's': {
            const char *s = va_arg(ap, const char *);
            int n = 0;
            if (!s) s = "(null)";
            while (s[n]) n++;
            put_padded(&o, s, n, width, left, ' ', 0);
            break; }
        case 'c': {
            char ch = (char)va_arg(ap, int);
            put_padded(&o, &ch, 1, width, left, ' ', 0);
            break; }
        case '%':
            put(&o, '%');
            break;
        default:
            put(&o, '%');
            put(&o, c);
            break;
        }
    }
    if (cap > 0) buf[o.len < cap ? o.len : cap - 1] = 0;
    return o.len;
}

int doomo_snprintf(char *buf, int cap, const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = doomo_vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
    return n;
}
