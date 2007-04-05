/*
 * MAX1110/1111 ADC chip emulation.
 *
 * Copyright (c) 2006 Openedhand Ltd.
 * Written by Andrzej Zaborowski <balrog@zabor.org>
 *
 * This code is licensed under the GPLv2.
 */

#include <vl.h>

struct max111x_s {
    void (*interrupt)(void *opaque);
    void *opaque;
    uint8_t tb1, rb2, rb3;
    int cycle;

    int input[8];
    int inputs, com;
};

/* Control-byte bitfields */
#define CB_PD0		(1 << 0)
#define CB_PD1		(1 << 1)
#define CB_SGL		(1 << 2)
#define CB_UNI		(1 << 3)
#define CB_SEL0		(1 << 4)
#define CB_SEL1		(1 << 5)
#define CB_SEL2		(1 << 6)
#define CB_START	(1 << 7)

#define CHANNEL_NUM(v, b0, b1, b2)	\
			((((v) >> (2 + (b0))) & 4) |	\
			 (((v) >> (3 + (b1))) & 2) |	\
			 (((v) >> (4 + (b2))) & 1))

uint32_t max111x_read(void *opaque)
{
    struct max111x_s *s = (struct max111x_s *) opaque;

    if (!s->tb1)
        return 0;

    switch (s->cycle ++) {
    case 1:
        return s->rb2;
    case 2:
        return s->rb3;
    }

    return 0;
}

/* Interpret a control-byte */
void max111x_write(void *opaque, uint32_t value)
{
    struct max111x_s *s = (struct max111x_s *) opaque;
    int measure, chan;

    /* Ignore the value if START bit is zero */
    if (!(value & CB_START))
        return;

    s->cycle = 0;

    if (!(value & CB_PD1)) {
        s->tb1 = 0;
        return;
    }

    s->tb1 = value;

    if (s->inputs == 8)
        chan = CHANNEL_NUM(value, 1, 0, 2);
    else
        chan = CHANNEL_NUM(value & ~CB_SEL0, 0, 1, 2);

    if (value & CB_SGL)
        measure = s->input[chan] - s->com;
    else
        measure = s->input[chan] - s->input[chan ^ 1];

    if (!(value & CB_UNI))
        measure ^= 0x80;

    s->rb2 = (measure >> 2) & 0x3f;
    s->rb3 = (measure << 6) & 0xc0;

    if (s->interrupt)
        s->interrupt(s->opaque);
}

struct max111x_s *max111x_init(void (*cb)(void *opaque), void *opaque)
{
    struct max111x_s *s;
    s = (struct max111x_s *)
            qemu_mallocz(sizeof(struct max111x_s));
    memset(s, 0, sizeof(struct max111x_s));

    s->interrupt = cb;
    s->opaque = opaque;

    /* TODO: add a user interface for setting these */
    s->input[0] = 0xf0;
    s->input[1] = 0xe0;
    s->input[2] = 0xd0;
    s->input[3] = 0xc0;
    s->input[4] = 0xb0;
    s->input[5] = 0xa0;
    s->input[6] = 0x90;
    s->input[7] = 0x80;
    s->com = 0;
    return s;
}

struct max111x_s *max1110_init(void (*cb)(void *opaque), void *opaque)
{
    struct max111x_s *s = max111x_init(cb, opaque);
    s->inputs = 8;
    return s;
}

struct max111x_s *max1111_init(void (*cb)(void *opaque), void *opaque)
{
    struct max111x_s *s = max111x_init(cb, opaque);
    s->inputs = 4;
    return s;
}

void max111x_set_input(struct max111x_s *s, int line, uint8_t value)
{
    if (line >= s->inputs) {
        printf("%s: There's no input %i\n", __FUNCTION__, line);
        return;
    }

    s->input[line] = value;
}