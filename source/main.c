// DS Rumble-Gyro ROM for GBA Flashcarts
//
// A from-scratch GBA homebrew ROM that drives the standard Game Boy Advance
// cartridge GPIO port (0x080000C4 DATA / 0x080000C6 DIRECTION / 0x080000C8
// CNT) exactly the way the official Drill Dozer (rumble) and WarioWare:
// Twisted! (gyro) cartridges do. It exists to exercise rumble+gyro capable
// GBA flashcart add-on hardware, and the same protocol is already decoded by
// the Analogue Pocket openfpga-GBA core's gba_gpioRTCSolarGyro.vhd (see
// README.md for the required cart_quirks.sv entry on that core).
//
// This ROM contains no code or data from any copyrighted game — only the
// public, documented GBA GPIO cartridge peripheral protocol.
//
// SPDX-License-Identifier: Apache-2.0

#include <gba_console.h>
#include <gba_video.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_input.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// GBA cartridge GPIO port (GBATEK "Game Pak I/O Port")
// ---------------------------------------------------------------------------
#define REG_GPIO_DATA (*(volatile unsigned short *)0x080000C4)
#define REG_GPIO_DIR  (*(volatile unsigned short *)0x080000C6)
#define REG_GPIO_CNT  (*(volatile unsigned short *)0x080000C8)

// DIRECTION value used by WarioWare: Twisted! — bit0/bit1 = gyro shift
// control (output), bit2 = gyro serial data (input), bit3 = rumble motor
// (output). Using the same value lets one ROM drive both peripherals.
#define GPIO_DIR_RUMBLE_GYRO 0x000B

#define GPIO_BIT_GYRO_RESET 0x01 // pulse: reload shift pointer to MSB (bit 15)
#define GPIO_BIT_GYRO_CLOCK 0x02 // pulse: advance shift pointer by one bit
#define GPIO_BIT_GYRO_DATA  0x04 // read-only: current shifted-out gyro bit
#define GPIO_BIT_RUMBLE     0x08 // read/write: rumble motor on/off

// Mirrors the last value written to REG_GPIO_DATA so the rumble bit can be
// held stable while the gyro shift-clock bits are pulsed, and vice versa —
// every DATA write re-asserts whichever bits aren't explicitly changed.
static unsigned short gpio_data_shadow = 0;

// Busy-wait a handful of cycles between GPIO pin transitions. Software
// bit-banging a 3-wire sensor interface at full GBA CPU speed can violate a
// real chip's minimum setup/hold time; this gives it room to settle.
static void gpio_settle(void)
{
    for (volatile int i = 0; i < 48; i++)
    {
    }
}

static void gpio_write(unsigned short bits)
{
    gpio_data_shadow = bits;
    REG_GPIO_DATA = bits;
    gpio_settle();
}

static void rumble_set(int on)
{
    if (on)
        gpio_write(gpio_data_shadow | GPIO_BIT_RUMBLE);
    else
        gpio_write((unsigned short)(gpio_data_shadow & ~GPIO_BIT_RUMBLE));
}

// Shifts out the full 16-bit gyro ADC sample, MSB first, by pulsing the
// shift clock 16 times and sampling the serial data bit after each step.
// This is the exact sequence WarioWare: Twisted! uses against the real
// gyro sensor chip. `raw_bits_out`, if non-NULL, receives each individual
// bit (0 or 1) in the order they were sampled, for on-screen diagnostics.
static unsigned short gyro_read16(char *raw_bits_out)
{
    unsigned short value = 0;
    unsigned short base = (unsigned short)(gpio_data_shadow & ~(GPIO_BIT_GYRO_RESET | GPIO_BIT_GYRO_CLOCK));

    gpio_write((unsigned short)(base | GPIO_BIT_GYRO_RESET));
    gpio_write(base);

    for (int i = 0; i < 16; i++)
    {
        gpio_settle();
        int bit = (REG_GPIO_DATA & GPIO_BIT_GYRO_DATA) ? 1 : 0;
        value = (unsigned short)((value << 1) | bit);
        if (raw_bits_out)
            raw_bits_out[i] = (char)('0' + bit);

        if (i != 15)
        {
            gpio_write((unsigned short)(base | GPIO_BIT_GYRO_CLOCK));
            gpio_write(base);
        }
    }

    return value;
}

int main(void)
{
    irqInit();
    irqEnable(IRQ_VBLANK);
    consoleDemoInit();

    REG_GPIO_CNT = 1; // enable GPIO port read-back (required for any read)
    REG_GPIO_DIR = GPIO_DIR_RUMBLE_GYRO;
    gpio_write(0);

    int rumble_latched = 0;

    iprintf("\x1b[1;1HDS Rumble-Gyro ROM for GBA Flashcarts\n");
    iprintf("\x1b[3;1HA     : toggle rumble latch\n");
    iprintf("\x1b[4;1HB held: rumble while held\n");
    iprintf("\x1b[5;1HSELECT: toggle raw-bit display\n");

    int show_raw = 0;

    while (1)
    {
        VBlankIntrWait();
        scanKeys();
        unsigned short keys_down = keysDown();
        unsigned short keys_held = keysHeld();

        if (keys_down & KEY_A)
            rumble_latched = !rumble_latched;
        if (keys_down & KEY_SELECT)
            show_raw = !show_raw;

        int rumble_active = rumble_latched || (keys_held & KEY_B);
        rumble_set(rumble_active);

        char raw_bits[17];
        unsigned short gyro = gyro_read16(raw_bits);
        raw_bits[16] = '\0';

        // Re-apply the rumble state after the gyro shift-clock pulses above,
        // in case a flashcart's GPIO implementation needs it re-latched
        // (real hardware does not, but this keeps behavior identical either
        // way).
        rumble_set(rumble_active);

        iprintf("\x1b[7;1HRumble : %s   \n", rumble_active ? "ON " : "OFF");
        iprintf("\x1b[8;1HGyro   : 0x%04X (%d)   \n", gyro, (int)gyro);
        if (show_raw)
        {
            iprintf("\x1b[9;1HRaw bits (MSB..LSB):\n");
            iprintf("\x1b[10;1H%s   \n", raw_bits);
            iprintf("\x1b[11;1HDIR=0x%04X CNT=0x%04X   \n", REG_GPIO_DIR, REG_GPIO_CNT);
        }
    }
}
