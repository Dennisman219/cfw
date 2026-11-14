# Building, flashing and recovery (UV-K1 / UV-K5(8) v3)

This document describes milestone 1: a firmware image that builds, boots and
shows a heartbeat on the display backlight. It does not use the display, keys,
radio, audio or external flash yet.

## Build

```sh
make
```

The default platform is `uv-k1-k5v3`. To build another platform later:

```sh
make PLATFORM=<platform-name>
```

Artifacts are written to `build/<platform>/`:

| File | Purpose |
| --- | --- |
| `cfw_uv-k1-k5v3.elf` | ELF with symbols and debug information |
| `cfw_uv-k1-k5v3.bin` | raw image to flash |
| `cfw_uv-k1-k5v3.hex` | Intel HEX image |
| `cfw_uv-k1-k5v3.elf.map` | linker map (useful when the image grows) |

`make verify` checks the generated `.bin` without hardware:

- initial stack pointer is inside the 16 KiB RAM,
- reset handler is inside the application flash area and has the Thumb bit set,
- the image fits in the 118 KiB application area.

## Memory layout

The radio keeps the stock bootloader in the first 10 KiB of the 128 KiB flash.
The bootloader starts the application at `0x08002800`.

| Region | Address range | Size | Contents |
| --- | --- | --- | --- |
| Bootloader | `0x08000000` - `0x080027FF` | 10 KiB | stock, not part of this image |
| Application flash | `0x08002800` - `0x0801FFFF` | 118 KiB | vector table, code, constants |
| RAM | `0x20000000` - `0x20003FFF` | 16 KiB | `.data`, `.bss`, heap, stack |

The reset vector in the application vector table initially points at
`Reset_Handler`. The startup file sets the stack pointer, calls `SystemInit()`,
copies `.data` from flash to RAM, zeroes `.bss`, runs C++ static constructors
and calls `main()`.

`SystemInit()` sets `SCB->VTOR` to `0x08002800`. Without that write the
Cortex-M0+ would keep using the bootloader's vector table, so interrupts from
the application would jump into the wrong code. This is one of the most common
reasons a custom firmware builds but immediately fails on the radio.

### Clock assumption

The stock bootloader configures the PLL and leaves the core running at 48 MHz.
Milestone 1 does not reprogram the clock. `SystemCoreClock` is initialized to
`48000000` in `platforms/uv-k1-k5v3/boot/system_py32f071.c` and is used to set
the SysTick reload value to `48000` ticks (1 ms).

If the bootloader is ever replaced, or a different startup path is used, the
clock configuration must be revisited first.

## Expected boot behavior

After reset and a successful boot, the display backlight flashes:

- on for about 50 ms,
- once per second,
- continuously.

This heartbeat proves that the reset vector, vector-table relocation, clock,
SysTick interrupt, main loop and backlight GPIO are working.

A hard fault instead produces a fast blink (roughly 5 Hz, 100 ms on/off). The
fault handler configures the backlight pin itself, so it also works if the
fault happens before normal board initialization.

If the backlight stays off, check in this order:

1. the image was flashed at `0x08002800`, not at `0x08000000`,
2. the bootloader still jumps to the application,
3. the radio is a PY32F071xB-based UV-K1 / UV-K5(8) v3,
4. the build actually contains the heartbeat code (`make size`, `nm`),
5. SWD is available to inspect `SCB->VTOR`, the stack pointer and the fault
   status registers.

The bootloader's clock configuration is assumed to be unchanged. A wrong clock
would make the heartbeat too fast or too slow rather than preventing it.

## Flashing

Automatic flashing is not set up yet. Use SWD with your preferred probe and
tool (for example OpenOCD, pyOCD or STM32CubeProgrammer if it supports the
PY32 device).

Rules for this milestone:

- flash only the application area starting at `0x08002800`,
- do not erase or overwrite the bootloader in the first 10 KiB,
- do not write the external PY25Q16 flash; it contains calibration and channel
  data used by the stock firmware,
- keep a full flash dump and a calibration backup before the first write.

The `.bin` file starts with the application vector table. If your tool needs an
explicit load address, use `0x08002800`.

The ELF is linked with a small segment alignment (`-z max-page-size=8`). Without
it, the first load segment would be aligned down to `0x08002000` and a tool
that flashes ELF segments could overwrite the last 2 KiB of the bootloader.
With the alignment the ELF only covers the application area. Flashing the
`.bin` at `0x08002800` is still the most explicit and preferred method.

## Recovery

Because the stock bootloader is not modified, a bad application image should
not make the radio unrecoverable. The normal recovery path is to connect SWD
and flash a known-good image again, or erase only the application area.

If the bootloader itself is erased, recovery requires the vendor/stock
firmware flashing procedure, which is outside the scope of this milestone.
That is why the bootloader area is deliberately excluded from the linker
script and from the flashing instructions.

## Design notes for the next milestone

The portable interface currently consists of `platform::init()` and
`platform::poll()` in `src/2_hal/Platform.h`. The UV-K5v3 implementation wires
them to the board and timebase. Later milestones will replace the direct
`poll()` call in `main.cpp` with the application framework while keeping the
same platform boundary.
