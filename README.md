# cfw

Custom C++ firmware for hackable PTT radios. The first supported radio is the
Quansheng UV-K1 / UV-K5(8) v3, which uses a Puya PY32F071xB microcontroller.

The project is also a learning environment for embedded C++. Portable code
lives in `src/` and talks to hardware through small interfaces. Each radio
provides its own implementation under `platforms/`. A simulator platform is
planned so applications and logic can be developed without hardware.

## Current status

Milestone 1 (platform build and boot) is implemented and build-verified:

- the portable entry point and the UV-K1 / UV-K5(8) v3 platform build with one
  `make` command,
- the vector table is relocated to the application address `0x08002800`,
- a 1 ms SysTick timebase runs from the bootloader-provided 48 MHz clock,
- a hard fault shows a fast (~5 Hz) backlight blink,
- `.elf`, `.bin`, `.hex`, `.map` and size information are produced.

Hardware verification is still pending. The build currently has no
normal-operation visual indicator, so confirm booting with SWD (or by
temporarily adding an indicator) once a probe is available. Display, keypad,
radio, the application framework, settings and the simulator are not part of
this milestone yet.

## Quick start

Prerequisites:

- `arm-none-eabi-gcc` / `g++` / `objcopy` / `size` (Arm GNU Toolchain),
- GNU Make,
- Python 3 (only for `make verify`).

```sh
make                 # build the default platform and print the size
make verify          # sanity-check the generated .bin
make size            # show section sizes
make clean
```

Build output:

```text
build/uv-k1-k5v3/
  cfw_uv-k1-k5v3.elf
  cfw_uv-k1-k5v3.bin    <- image to flash
  cfw_uv-k1-k5v3.hex
  cfw_uv-k1-k5v3.elf.map
  obj/                  <- object and dependency files
```

See [docs/dev/build-and-flash.md](docs/dev/build-and-flash.md) for the memory
layout, boot sequence and recovery notes.

## Repository layout

```text
src/                        portable firmware
  2_hal/Platform.h          interface every platform implements
  main.cpp                  portable entry point

platforms/uv-k1-k5v3/       UV-K1 / UV-K5(8) v3 platform
  2_hal/                    implementation of src/2_hal interfaces
  3_hardware/               timebase, fault indicator
  4_cmsis/                  vendor CMSIS and Puya LL headers
  boot/                     startup file, system setup, linker script
  Makefile                  platform flags and source list

scripts/verify_image.py     checks the vector table and image size
docs/                       user and developer documentation
```

Adding a portable `.c`/`.cpp` file under `src/` includes it in the build
automatically. Platform files are listed explicitly in the platform Makefile,
so incomplete or hardware-specific experiments do not leak into other builds.
