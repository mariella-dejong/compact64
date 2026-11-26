// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Unit tests of the 6526 model for what the Lorenz test suite does not cover:
// reset state, ports, ~PC, ~FLAG, time of day and the serial port. Expected
// behaviour from the 6526 data sheet (MOS, 1982).

#include <stdio.h>
#include <string.h>

#include "cia6526.h"

static int failures, checks;

#define CHECK(cond)                                                     \
    do {                                                                \
        checks++;                                                       \
        if (!(cond)) {                                                  \
            failures++;                                                 \
            printf("%s:%d: %s: FAIL %s\n", __FILE__, __LINE__, __func__, #cond); \
        }                                                               \
    } while (0)

#define CHECK_EQ(a, b)                                                  \
    do {                                                                \
        checks++;                                                       \
        const unsigned va = (unsigned)(a), vb = (unsigned)(b);          \
        if (va != vb) {                                                 \
            failures++;                                                 \
            printf("%s:%d: %s: FAIL %s == 0x%02x, expected 0x%02x\n", __FILE__, __LINE__, \
                   __func__, #a, va, vb);                               \
        }                                                               \
    } while (0)

static void cycles(cia_t *c, int n) {
    while (n-- > 0) {
        cia_clock(c);
    }
}

static void test_reset(void) {
    cia_t c;
    cia_init(&c, CIA_MODEL_6526);
    CHECK_EQ(cia_peek(&c, CIA_DDRA), 0x00);
    CHECK_EQ(cia_peek(&c, CIA_DDRB), 0x00);
    CHECK_EQ(cia_pa_out(&c), 0xFF);  // all inputs, pulled up
    CHECK_EQ(cia_pb_out(&c), 0xFF);
    CHECK_EQ(cia_peek(&c, CIA_TALO), 0xFF);
    CHECK_EQ(cia_peek(&c, CIA_TAHI), 0xFF);
    CHECK_EQ(cia_peek(&c, CIA_CRA), 0x00);
    CHECK_EQ(cia_peek(&c, CIA_CRB), 0x00);
    CHECK_EQ(cia_peek(&c, CIA_ICR), 0x00);
    CHECK(!cia_irq(&c));
    CHECK_EQ(cia_peek(&c, CIA_TODHR), 0x01);
}

static void test_ports(void) {
    cia_t c;
    cia_init(&c, CIA_MODEL_6526);
    cia_write(&c, CIA_DDRA, 0x0F);
    cia_write(&c, CIA_PRA, 0x05);
    CHECK_EQ(cia_pa_out(&c), 0xF5);  // inputs high, outputs as written
    c.pa_pins = 0x35;
    CHECK_EQ(cia_peek(&c, CIA_PRA), 0x35);  // a read returns the pins
    CHECK_EQ(cia_peek(&c, CIA_DDRA), 0x0F);

    // PB6/PB7 carry the timer outputs when PBON is set, as outputs.
    cia_write(&c, CIA_DDRB, 0x00);
    cia_write(&c, CIA_CRA, 0x06);  // PBON, toggle, stopped
    CHECK_EQ(cia_pb_dir(&c) & 0xC0, 0x40);
    CHECK_EQ(cia_pb_out(&c) & 0x40, 0x00);  // toggle starts low after reset
    cia_write(&c, CIA_CRA, 0x07);           // start: rising START sets it
    CHECK_EQ(cia_pb_out(&c) & 0x40, 0x40);
}

static void test_pc(void) {
    cia_t c;
    cia_init(&c, CIA_MODEL_6526);
    cia_clock(&c);
    CHECK(!c.pc);
    (void)cia_read(&c, CIA_PRB);
    CHECK(!c.pc);  // not yet in the cycle of the access
    cia_clock(&c);
    CHECK(c.pc);   // low for the next cycle
    cia_clock(&c);
    CHECK(!c.pc);
    cia_write(&c, CIA_PRB, 0x12);
    cia_clock(&c);
    CHECK(c.pc);
    (void)cia_read(&c, CIA_PRA);  // port A does not pulse ~PC
    cia_clock(&c);
    CHECK(!c.pc);
}

static void test_flag(void) {
    cia_t c;
    cia_init(&c, CIA_MODEL_6526);
    cia_write(&c, CIA_ICR, 0x90);
    c.flag_in = false;  // falling edge
    cia_clock(&c);
    CHECK_EQ(cia_peek(&c, CIA_ICR) & 0x1F, CIA_INT_FLAG);
    cia_clock(&c);
    CHECK_EQ(cia_peek(&c, CIA_ICR), 0x90);
    cia_clock(&c);
    CHECK(cia_irq(&c));
    (void)cia_read(&c, CIA_ICR);
    cycles(&c, 3);  // still low, no new edge
    CHECK_EQ(cia_peek(&c, CIA_ICR), 0x00);
    CHECK(!cia_irq(&c));
}

static void tod_ticks(cia_t *c, int tenths, int per_tenth) {
    for (int i = 0; i < tenths * per_tenth; i++) {
        cia_tod_tick(c);
    }
}

static void test_tod(void) {
    cia_t c;
    cia_init(&c, CIA_MODEL_6526);
    cia_write(&c, CIA_CRA, 0x80);  // 50 Hz: five pin edges per tenth

    // Writing the hours stops the clock until the tenths are written.
    cia_write(&c, CIA_TODHR, 0x11);
    cia_write(&c, CIA_TODMIN, 0x59);
    cia_write(&c, CIA_TODSEC, 0x59);
    tod_ticks(&c, 3, 5);
    CHECK_EQ(cia_peek(&c, CIA_TOD10), 0x00);
    cia_write(&c, CIA_TOD10, 0x08);
    tod_ticks(&c, 1, 5);
    CHECK_EQ(cia_peek(&c, CIA_TOD10), 0x09);

    // 11:59:59.9 AM to 12:00:00.0 PM.
    tod_ticks(&c, 1, 5);
    CHECK_EQ(cia_peek(&c, CIA_TODHR), 0x92);
    CHECK_EQ(cia_peek(&c, CIA_TODMIN), 0x00);
    CHECK_EQ(cia_peek(&c, CIA_TODSEC), 0x00);
    CHECK_EQ(cia_peek(&c, CIA_TOD10), 0x00);

    // Reading the hours latches all four until the tenths are read.
    (void)cia_read(&c, CIA_TODHR);
    tod_ticks(&c, 12, 5);
    CHECK_EQ(cia_read(&c, CIA_TODSEC), 0x00);
    CHECK_EQ(cia_read(&c, CIA_TOD10), 0x00);
    CHECK_EQ(cia_read(&c, CIA_TODSEC), 0x01);
    CHECK_EQ(cia_read(&c, CIA_TOD10), 0x02);

    // 60 Hz: six edges per tenth.
    cia_write(&c, CIA_CRA, 0x00);
    cia_write(&c, CIA_TODHR, 0x01);
    cia_write(&c, CIA_TOD10, 0x00);
    tod_ticks(&c, 1, 5);
    CHECK_EQ(cia_peek(&c, CIA_TOD10), 0x00);
    cia_tod_tick(&c);
    CHECK_EQ(cia_peek(&c, CIA_TOD10), 0x01);

    // 12:59:59.9 PM to 1:00:00.0 PM.
    cia_write(&c, CIA_CRA, 0x80);
    cia_write(&c, CIA_TODHR, 0x92 ^ 0x80);  // writing 12 inverts AM/PM
    CHECK_EQ(cia_peek(&c, CIA_TODHR), 0x92);
    cia_write(&c, CIA_TODMIN, 0x59);
    cia_write(&c, CIA_TODSEC, 0x59);
    cia_write(&c, CIA_TOD10, 0x09);
    tod_ticks(&c, 1, 5);
    CHECK_EQ(cia_peek(&c, CIA_TODHR), 0x81);
}

static void test_alarm(void) {
    cia_t c;
    cia_init(&c, CIA_MODEL_6526);
    cia_write(&c, CIA_CRA, 0x80);
    cia_write(&c, CIA_CRB, 0x80);  // writes go to the alarm
    cia_write(&c, CIA_TODHR, 0x01);
    cia_write(&c, CIA_TODMIN, 0x00);
    cia_write(&c, CIA_TODSEC, 0x01);
    cia_write(&c, CIA_TOD10, 0x00);
    cia_write(&c, CIA_CRB, 0x00);
    CHECK_EQ(cia_peek(&c, CIA_TODSEC), 0x00);  // the time is unchanged
    // While the alarm is written one register at a time it passed through
    // the current time (1:00:00.0), which the comparator catches.
    (void)cia_read(&c, CIA_ICR);
    cia_write(&c, CIA_ICR, 0x84);
    tod_ticks(&c, 9, 5);
    CHECK_EQ(cia_peek(&c, CIA_ICR) & 0x04, 0x00);
    tod_ticks(&c, 1, 5);
    CHECK_EQ(cia_peek(&c, CIA_ICR) & 0x04, 0x04);
    cycles(&c, 2);  // ICR bit 7 one cycle after the source
    CHECK_EQ(cia_peek(&c, CIA_ICR), 0x84);
    CHECK(!cia_irq(&c));
    cia_clock(&c);  // the pin one cycle after bit 7
    CHECK(cia_irq(&c));
}

static void test_serial_out(void) {
    cia_t c;
    cia_init(&c, CIA_MODEL_6526);
    cia_write(&c, CIA_TALO, 0x01);
    cia_write(&c, CIA_TAHI, 0x00);
    cia_write(&c, CIA_CRA, 0x51);  // serial out, force load, start
    cia_write(&c, CIA_SDR, 0xA5);
    uint8_t got = 0;
    int bits = 0;
    bool prev_cnt = c.cnt_out;
    for (int i = 0; i < 200 && bits < 8; i++) {
        cia_clock(&c);
        if (c.cnt_out && !prev_cnt) {  // receivers sample on the rising edge
            got = (uint8_t)((got << 1) | (c.sp_out ? 1 : 0));
            bits++;
        }
        prev_cnt = c.cnt_out;
    }
    CHECK_EQ(bits, 8);
    CHECK_EQ(got, 0xA5);
    cycles(&c, 4);
    CHECK_EQ(cia_peek(&c, CIA_ICR) & CIA_INT_SP, CIA_INT_SP);
    CHECK(c.cnt_out);  // CNT idles high
}

static void test_serial_in(void) {
    cia_t c;
    cia_init(&c, CIA_MODEL_6526);
    const uint8_t byte = 0x3C;
    for (int b = 7; b >= 0; b--) {
        c.sp_in = (byte >> b) & 1;
        c.cnt_in = false;
        cia_clock(&c);
        c.cnt_in = true;  // shifted in on the rising edge
        cia_clock(&c);
    }
    CHECK_EQ(cia_peek(&c, CIA_SDR), byte);
    CHECK_EQ(cia_peek(&c, CIA_ICR) & CIA_INT_SP, CIA_INT_SP);
}

static void test_timer_basics(void) {
    cia_t c;
    cia_init(&c, CIA_MODEL_6526);
    cia_write(&c, CIA_TALO, 0x03);
    cia_write(&c, CIA_TAHI, 0x00);  // stopped: loads the counter
    cia_clock(&c);
    CHECK_EQ(cia_peek(&c, CIA_TALO), 0x03);
    cia_write(&c, CIA_ICR, 0x81);
    cia_write(&c, CIA_CRA, 0x01);
    // Continuous mode with latch 3 underflows every 4 cycles.
    int underflows = 0;
    for (int i = 0; i < 40; i++) {
        cia_clock(&c);
        underflows += c.ta.underflow;
    }
    CHECK_EQ(underflows, 9);  // the first after the 2-cycle start delay
    CHECK(cia_irq(&c));
    CHECK_EQ(cia_read(&c, CIA_ICR), 0x81);
    CHECK_EQ(cia_peek(&c, CIA_ICR), 0x00);
}

int main(void) {
    test_reset();
    test_ports();
    test_pc();
    test_flag();
    test_tod();
    test_alarm();
    test_serial_out();
    test_serial_in();
    test_timer_basics();
    printf("unit: %d of %d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
