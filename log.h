#ifndef LOG_H
#define LOG_H

#include <stdint.h>

#ifndef LOG_BASE
#define LOG_BASE 0x00022000u
#endif
#ifndef LOG_SIZE
#define LOG_SIZE 0x2000u            /* header + data */
#endif

#define LOG_MAGIC 0x31474F4Cu       /* bytes 'L' 'O' 'G' '1' */

struct log_hdr {
    volatile uint32_t magic;
    volatile uint32_t size;
    volatile uint32_t head;
    volatile uint32_t total;
    volatile uint32_t state;
    volatile uint32_t seq;
    volatile uint8_t  data[];
};

#define LOGBUF ((struct log_hdr *)(LOG_BASE))

static inline void log_init(void)
{
    struct log_hdr *l = LOGBUF;
    l->magic = LOG_MAGIC;
    l->size  = LOG_SIZE - sizeof(struct log_hdr);
    l->head  = 0;
    l->total = 0;
    l->seq   = 0;
    l->state = 1;
}

static inline void log_done(void)
{
    LOGBUF->state = 2;
}

static inline void log_putc(char c)
{
    struct log_hdr *l = LOGBUF;
    uint32_t h = l->head;
    l->data[h] = (uint8_t)c;
    if (++h >= l->size)
        h = 0;
    l->head = h;
    l->total++;
}

static inline void log_str(const char *s)
{
    while (*s)
        log_putc(*s++);
}

static inline void log_hex(uint32_t v)          /* 8 hex digits */
{
    int i;
    for (i = 28; i >= 0; i -= 4)
        log_putc("0123456789ABCDEF"[(v >> i) & 15u]);
}

static inline void log_dec(uint32_t v)
{
    static const uint32_t p10[10] = {
        1000000000u, 100000000u, 10000000u, 1000000u, 100000u,
        10000u, 1000u, 100u, 10u, 1u
    };
    int i, started = 0;
    for (i = 0; i < 10; i++) {
        char d = '0';
        while (v >= p10[i]) {
            v -= p10[i];
            d++;
        }
        if (d != '0' || started || i == 9) {
            log_putc(d);
            started = 1;
        }
    }
}

/* start a new line: "[seq] " */
static inline void log_begin(void)
{
    struct log_hdr *l = LOGBUF;
    l->seq = l->seq + 1;
    log_putc('[');
    log_dec(l->seq);
    log_putc(']');
    log_putc(' ');
}

/* "[seq] text\n" */
static inline void log_msg(const char *text)
{
    log_begin();
    log_str(text);
    log_putc('\n');
}

/* "[seq] text 123\n" */
static inline void log_val(const char *text, uint32_t v)
{
    log_begin();
    log_str(text);
    log_putc(' ');
    log_dec(v);
    log_putc('\n');
}

/* "[seq] text 0x0000ABCD\n" */
static inline void log_valx(const char *text, uint32_t v)
{
    log_begin();
    log_str(text);
    log_str(" 0x");
    log_hex(v);
    log_putc('\n');
}

#endif /* LOG_H */
