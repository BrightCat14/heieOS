#include <stdint.h>
#include "log.h"

#ifndef SCAN_MODE
#define SCAN_MODE 0
#endif

#define PIO_BASE 0x0300B000u
#define PORT_C   (PIO_BASE + 0x48u)
#define PORT_F   (PIO_BASE + 0xB4u)
#define PORT_G   (PIO_BASE + 0xD8u)
#define PORT_H   (PIO_BASE + 0xFCu)
#define PORT_I   (PIO_BASE + 0x120u)

#define LED_PINS ((1u << 4) | (1u << 5) | (1u << 6))   /* PC4-6 and PH4-6 */

#define BTN_PORT        0       /* PORT_C/F/G/H/I, or 0 = not configured yet */
#define BTN_PIN         0       /* 0..31 */
#define BTN_ACTIVE_LOW  1       /* 1 = pressing pulls the pin to GND */

#define BLINK_POLLS     300000u /* half a blink period */
#define RUN_BLINKS      6u      /* leave by ourselves after this many blinks */
#define BTN_ARM_POLLS   20000u  /* button must be seen released this long first */
#define BTN_HOLD_POLLS  20000u  /* ...and pressed this long to count */

#define SCAN_POLLS      1500000u
#define SCAN_MAX_EVENTS 300u
#define SCAN_PULL       1       /* 1 = pull-up (button to GND), 2 = pull-down (button to 3.3 V) */

void __attribute__((naked, section(".text.boot"))) _start(void)
{
    __asm__ volatile("b main");
}

static inline volatile uint32_t *reg(uint32_t port, uint32_t off)
{
    return (volatile uint32_t *)(port + off);
}
#define DAT(port) reg((port), 0x10u)

static void pin_cfg(uint32_t port, uint32_t pin, uint32_t fn)   /* 0 = in, 1 = out */
{
    volatile uint32_t *r = reg(port, 4u * (pin / 8u));
    uint32_t sh = (pin % 8u) * 4u;
    *r = (*r & ~(0xFu << sh)) | (fn << sh);
}

static void pin_pull(uint32_t port, uint32_t pin, uint32_t mode) /* 0 off, 1 up, 2 down */
{
    volatile uint32_t *r = reg(port, 0x1Cu + 4u * (pin / 16u));
    uint32_t sh = (pin % 16u) * 2u;
    *r = (*r & ~(3u << sh)) | (mode << sh);
}

static void burn(uint32_t n)
{
    while (n--)
        __asm__ volatile("nop");
}

#if SCAN_MODE

struct scan_port {
    char     letter;
    uint32_t base;
    uint32_t skip;      /* pins we must not touch */
};

static const struct scan_port ports[] = {
    { 'C', PORT_C, 0 },
    { 'F', PORT_F, 0 },
    { 'G', PORT_G, 0 },
    { 'H', PORT_H, 0 },
    { 'I', PORT_I, 0 },
};
#define NPORTS (sizeof(ports) / sizeof(ports[0]))

int main(void)
{
    uint32_t prev[NPORTS];
    uint32_t i, p, polls, events = 0;

    log_init();
    log_msg("pin finder started");

    for (i = 0; i < NPORTS; i++) {
        for (p = 0; p < 32; p++) {
            if (ports[i].skip & (1u << p))
                continue;
            pin_cfg(ports[i].base, p, 0);
            pin_pull(ports[i].base, p, SCAN_PULL);
        }
    }
    burn(50000);                                    /* let the pulls settle */

    for (i = 0; i < NPORTS; i++) {
        prev[i] = *DAT(ports[i].base) & ~ports[i].skip;
        log_begin();
        log_putc('P');
        log_putc(ports[i].letter);
        log_str(" start 0x");
        log_hex(prev[i]);
        log_putc('\n');
    }

    for (polls = 0; polls < SCAN_POLLS && events < SCAN_MAX_EVENTS; polls++) {
        for (i = 0; i < NPORTS; i++) {
            uint32_t cur  = *DAT(ports[i].base) & ~ports[i].skip;
            uint32_t diff = cur ^ prev[i];
            if (!diff)
                continue;
            for (p = 0; p < 32; p++) {
                if (!(diff & (1u << p)))
                    continue;
                log_begin();
                log_putc('P');
                log_putc(ports[i].letter);
                log_dec(p);
                log_putc('=');
                log_putc((cur >> p) & 1u ? '1' : '0');
                log_putc('\n');
                events++;
            }
            prev[i] = cur;
        }
    }

    log_val("pin changes seen", events);
    log_msg("pin finder done");
    log_done();
    return 0;                                       /* back to FEL */
}

#else
#if BTN_PORT == PORT_C
#define LED_C (LED_PINS & ~(1u << BTN_PIN))
#else
#define LED_C LED_PINS
#endif
#if BTN_PORT == PORT_H
#define LED_H (LED_PINS & ~(1u << BTN_PIN))
#else
#define LED_H LED_PINS
#endif

static void leds_init(void)
{
    uint32_t p;
    for (p = 4; p <= 6; p++) {
        if (LED_C & (1u << p))
            pin_cfg(PORT_C, p, 1);
        if (LED_H & (1u << p))
            pin_cfg(PORT_H, p, 1);
    }
}

static void leds_set(int on)
{
    volatile uint32_t *c = DAT(PORT_C);
    volatile uint32_t *h = DAT(PORT_H);
    if (on) {
        *c |= LED_C;
        *h |= LED_H;
    } else {
        *c &= ~LED_C;
        *h &= ~LED_H;
    }
}

static void btn_init(void)
{
#if BTN_PORT
    pin_cfg(BTN_PORT, BTN_PIN, 0);
    pin_pull(BTN_PORT, BTN_PIN, BTN_ACTIVE_LOW ? 1u : 2u);
    log_val("button on pin", BTN_PIN);
#else
    log_msg("no button configured (BTN_PORT is 0)");
#endif
}

static int btn_pressed(void)
{
#if BTN_PORT
    uint32_t level = (*DAT(BTN_PORT) >> BTN_PIN) & 1u;
    return BTN_ACTIVE_LOW ? !level : (int)level;
#else
    (void)*DAT(PORT_C);     /* keep the loop timing the same as with a real button */
    return 0;
#endif
}

struct btn {
    uint32_t idle;          /* consecutive "released" polls */
    uint32_t held;          /* consecutive "pressed" polls */
    int      armed;         /* has been seen released, so a press is real */
};

/* returns 1 once the button has been held long enough */
static int btn_poll(struct btn *b)
{
    if (!btn_pressed()) {
        b->held = 0;
        if (!b->armed && ++b->idle >= BTN_ARM_POLLS) {
            b->armed = 1;
            log_msg("button armed");
        }
        return 0;
    }
    b->idle = 0;
    return b->armed && ++b->held >= BTN_HOLD_POLLS;
}

/* wait n polls; returns 1 if the button asked us to quit */
static int wait_polls(uint32_t n, struct btn *b)
{
    while (n--) {
        if (btn_poll(b))
            return 1;
        __asm__ volatile("nop");
    }
    return 0;
}

int main(void)
{
    struct btn b = { 0, 0, 0 };
    uint32_t n;
    int quit = 0;

    log_init();
    log_msg("boot");
    leds_init();
    btn_init();

    for (n = 0; n < RUN_BLINKS; n++) {
        log_val("blink", n);

        leds_set(1);
        quit = wait_polls(BLINK_POLLS, &b);
        if (quit)
            break;

        leds_set(0);
        quit = wait_polls(BLINK_POLLS, &b);
        if (quit)
            break;
    }

    log_msg(quit ? "button pressed, leaving" : "run limit reached, leaving");
    log_done();
    leds_set(1);         
    return 0;            
}
#endif
