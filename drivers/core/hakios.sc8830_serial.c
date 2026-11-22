// language: C, file: drivers/core/hakios.sc8830_serial.c, target: Darwin/XNU ARMv7 Spreadtrum SC8830
// *Spreadtrum SC8830/SharkL UART early serial console and kernel putc hook*

#include <stdint.h>
#include <stdbool.h>

#define SC8830_UART1_BASE       0x70100000u
#define SC8830_UART0_BASE       0x70000000u

#define REG_UART_TXD            0x0000u  // TX data register
#define REG_UART_RXD            0x0004u  // RX data register
#define REG_UART_STS0           0x0008u  // Status register 0 (FIFO states)
#define REG_UART_STS1           0x000Cu  // Status register 1
#define REG_UART_IEN            0x0010u  // Interrupt enable
#define REG_UART_ICLR           0x0014u  // Interrupt clear
#define REG_UART_CTL0           0x0018u  // Control register 0
#define REG_UART_CLKD0          0x001Cu  // Clock divisor LSB
#define REG_UART_CLKD1          0x0020u  // Clock divisor MSB

#define STS0_TX_FIFO_BUSY       (1u << 8)
#define STS0_TX_FIFO_EMPTY      (1u << 9)
#define STS0_RX_FIFO_AVAIL      (1u << 0)

static volatile uint32_t * const g_uart_base = (volatile uint32_t *)SC8830_UART1_BASE;

static inline void mmio_write32(uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)(SC8830_UART1_BASE + reg) = val;
}

static inline uint32_t mmio_read32(uint32_t reg) {
    return *(volatile uint32_t *)(SC8830_UART1_BASE + reg);
}

void hakios_sc8830_uart_init(void) {
    // SC8830 UART1 initialized by S-Boot at 115200 8N1 (clk=26MHz, divisor=14)
    // Clear pending interrupts
    mmio_write32(REG_UART_ICLR, 0xFFFFFFFFu);
    // Ensure 8 data bits, 1 stop bit, no parity (8N1)
    mmio_write32(REG_UART_CTL0, 0x0000001Cu);
}

void hakios_sc8830_uart_putc(char c) {
    if (c == '\n') {
        hakios_sc8830_uart_putc('\r');
    }
    // Wait until TX FIFO is not full / not busy
    while (mmio_read32(REG_UART_STS0) & STS0_TX_FIFO_BUSY) {
        // spin
    }
    mmio_write32(REG_UART_TXD, (uint32_t)(uint8_t)c);
}

void hakios_sc8830_uart_puts(const char *str) {
    if (!str) return;
    while (*str) {
        hakios_sc8830_uart_putc(*str++);
    }
}

// Early Darwin putc hook for PE_kputc
void sc8830_early_putc(char c) {
    hakios_sc8830_uart_putc(c);
}
