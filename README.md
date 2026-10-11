# DS Rumble-Gyro ROM for GBA Flashcarts

A from-scratch, original GBA homebrew ROM that drives the standard **Game
Boy Advance cartridge GPIO port** — the same hardware interface used by the
official Drill Dozer (rumble) and WarioWare: Twisted! (gyro) cartridges —
to let a GBA flashcart's rumble/gyro add-on hardware be tested, and to let
the Analogue Pocket `openfpga-GBA` core respond to the same inputs over its
GPIO emulation.

This ROM contains **no code or data from any licensed game**. It is an
independent implementation of the publicly documented GBA GPIO cartridge
peripheral protocol (GBATEK "Game Pak I/O Port").

## What it does

- Press **A** to latch rumble on/off.
- Hold **B** to rumble only while held.
- The screen continuously shows the current rumble state and the raw
  16-bit value read back from the gyro sensor's serial shift register.

## The GPIO protocol it uses

| Register | Address | Purpose |
|---|---|---|
| DATA | `0x080000C4` | bit0/1 = gyro shift control (out), bit2 = gyro data (in), bit3 = rumble (out) |
| DIRECTION | `0x080000C6` | set to `0x0B` to enable rumble (bit3) + gyro (bits0-2) simultaneously |
| CNT | `0x080000C8` | write `1` once to enable read-back of the DATA register |

Rumble: with DIRECTION bit3 set as output, writing bit3 of DATA turns the
motor on (`1`) or off (`0`).

Gyro: pulse DATA bit0 once to reset the sensor's internal shift pointer to
the MSB, then 16 times read DATA bit2 (current bit) and pulse DATA bit1
(advance to the next bit) — yielding the full 16-bit ADC sample, MSB first.
This is exactly the sequence WarioWare: Twisted! uses.

See [source/main.c](source/main.c) for the full implementation.

## Hardware notes

Bit-banging the gyro shift register at full GBA CPU speed (no delay between
GPIO pin transitions) was tested and found to produce a constant, non-moving
reading on real GBA + flashcart hardware (confirmed on hardware explicitly
documented to support both rumble and gyro) — the sensor's output needs a
short settle time after each clock/reset pulse before it's valid. `main.c`
therefore busy-waits briefly (`gpio_settle()`) after every GPIO write. With
that delay in place, the gyro value tracks real-world tilt correctly on both
real hardware and the Analogue Pocket `openfpga-GBA` core (once that core is
rebuilt with the `cart_quirks.sv` entry below).

Press **SELECT** to toggle a raw-bit diagnostic view (shows the individual
16 sampled bits plus the `DIRECTION`/`CNT` register state) if you need to
debug a different piece of GPIO-gyro hardware.

## Building

Requires devkitARM (devkitPro):

```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=$DEVKITPRO/devkitARM
make
```

Produces `DS_Rumble-Gyro_ROM_for_GBAFlashcarts.gba`, ready to write to a
flashcart or load on a GBA/Analogue Pocket.

## Compatibility

- **Real GBA hardware / most flashcarts**: the GPIO port is physical
  cartridge hardware, so this ROM works on any GBA regardless of ROM header
  — only flashcarts with rumble and/or gyro add-on hardware will actually
  move/respond, but the ROM itself runs everywhere.
- **Analogue Pocket `openfpga-GBA` core**: the core only enables its GPIO
  emulation (and therefore its own DS-Rumble-Pak-style physical rumble
  driver) for cart IDs it recognizes as needing it, to save FPGA resources.
  This ROM's header game code is `ZFDE`, and
  `src/fpga/core/cart_quirks.sv` in `openfpga-GBA` has a matching
  `gpio_quirk` entry (3-char match on `"ZFD"`) so the Pocket responds to it
  the same way a real GBA/flashcart would.
- **k3v-gba**: out of scope for this project; that core currently has the
  GPIO rumble/gyro/tilt/solar peripheral stripped out entirely to save FPGA
  area, so this ROM will not produce rumble/gyro feedback there.

## License

Apache License 2.0 — see [LICENSE](LICENSE). This is wholly original code;
no third-party ROM data is included or required.
