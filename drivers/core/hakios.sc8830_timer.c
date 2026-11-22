// language: C, file: drivers/core/hakios.sc8830_timer.c, target: Darwin/XNU ARMv7 Spreadtrum SC8830
// *ARM Cortex-A7 Generic Architecture Timer (CNTP_TVAL, CNTP_CTL) for Darwin clock tick*

#include <stdint.h>
#include <stdbool.h>

#define CNTP_CTL_ENABLE         (1u << 0)
#define CNTP_CTL_IMASK          (1u << 1)
#define CNTP_CTL_ISTATUS        (1u << 2)

#define SC8830_TIMER_FREQ       26000000u // 26 MHz system oscillator
#define DARWIN_HZ               100u       // 100 Hz scheduler quantum (10ms tick)
#define TICKS_PER_QUANTUM       (SC8830_TIMER_FREQ / DARWIN_HZ) // 260,000 ticks

static inline void armv7_write_cntp_tval(uint32_t val) {
    __asm__ volatile("mcr p15, 0, %0, c14, c2, 0" : : "r"(val));
}

static inline void armv7_write_cntp_ctl(uint32_t val) {
    __asm__ volatile("mcr p15, 0, %0, c14, c2, 1" : : "r"(val));
}

static inline uint32_t armv7_read_cntp_ctl(void) {
    uint32_t val;
    __asm__ volatile("mrc p15, 0, %0, c14, c2, 1" : "=r"(val));
    return val;
}

static inline uint64_t armv7_read_cntpct(void) {
    uint64_t val;
    __asm__ volatile("mrrc p15, 0, %Q0, %R0, c14" : "=r"(val));
    return val;
}

void hakios_sc8830_timer_init(void) {
    // Disable timer while reconfiguring
    armv7_write_cntp_ctl(0);
    // Program TVAL count
    armv7_write_cntp_tval(TICKS_PER_QUANTUM);
    // Enable timer, unmask interrupt
    armv7_write_cntp_ctl(CNTP_CTL_ENABLE);
}

void hakios_sc8830_timer_ack(void) {
    // Reload timer for next quantum
    armv7_write_cntp_tval(TICKS_PER_QUANTUM);
}

uint64_t hakios_sc8830_timer_now(void) {
    return armv7_read_cntpct();
}
