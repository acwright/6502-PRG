6502-PRG
========

A 6502 assembly language program template for the [AC6502](https://github.com/acwright/6502-ACE) family of computer systems.

> 📖 **Guide:** [AC6502 Documentation](https://acwright.github.io/6502-DOCS/) — the user's and programmer's guide for the whole family.
> This template is walked through end to end in [Starting from a template](https://acwright.github.io/6502-DOCS/crossdev/templates).

## Overview

Programs for this system are loaded into RAM at `$0800` and executed from BASIC. Unlike cartridges (which replace ROM), programs run entirely in RAM alongside the BIOS — the Kernal, the BASIC interpreter, Wozmon and, on BIOS 1.x, the Monitor — all of which remain available.

The template builds two ways from the same source: `make` for any ACE on BIOS 1.x with a TMS9918A, and `make VDP=1` for an ACE converted to a [6502-PICOVDP](https://github.com/acwright/6502-PICOVDP) on BIOS 2.x. See [Building for the 6502-PICOVDP](#building-for-the-6502-picovdp).

### How It Works

1. The program is loaded into RAM at `$0800` — normally with BASIC's `LOAD`, from CompactFlash or over serial
2. BASIC's `RUN` command executes the tokenized stub at the start of the file
3. The stub decodes to `10 SYS 2060`, which calls the machine code entry point at `$080C`
4. Your program runs with full access to the Kernal jump table
5. Return to BASIC with `RTS`

### Memory Layout

| Range | Contents |
|-------|----------|
| `$0000–$0039` | Zero page — system pointers, BASIC, and Kernal scratch (see `6502.inc` for details) |
| `$003A–$00FF` | Zero page — **free for user programs** (198 bytes) |
| `$0100–$01FF` | CPU stack |
| `$0200–$02FF` | Input ring buffer (managed by Kernal) |
| `$0300–$03FF` | Kernal variables (vectors, cursor, HW flags, etc.) |
| `$0400–$05FF` | BASIC's input line and tokenizing buffers |
| `$0600–$07FF` | CompactFlash sector buffer — clobbered by any filesystem call |
| `$0800–$080B` | **BASIC startup stub** (`10 SYS 2060`) |
| `$080C–$7FFF` | **Your program code and data** (~30 KB available) |
| `$8000–$9FFF` | I/O hardware registers |
| `$A000–$A0FF` | Kernal jump table (stable API) |

### BASIC Startup Stub

The first 12 bytes of every program file contain a tokenized BASIC line that serves as a loader:

| Address | Byte | Meaning |
|---------|------|---------|
| `$0800` | `$0A` | Next-line pointer lo → `$080A` |
| `$0801` | `$08` | Next-line pointer hi |
| `$0802` | `$0A` | Line number 10 (lo) |
| `$0803` | `$00` | Line number 10 (hi) |
| `$0804` | `$A5` | `TOK_SYS` |
| `$0805` | `$32` | `'2'` |
| `$0806` | `$30` | `'0'` |
| `$0807` | `$36` | `'6'` |
| `$0808` | `$30` | `'0'` |
| `$0809` | `$00` | Line terminator |
| `$080A` | `$00` | End-of-program sentinel (lo) |
| `$080B` | `$00` | End-of-program sentinel (hi) |
| `$080C` | … | **Machine code entry point** (`2060` decimal = `$080C`) |

### Kernal Services

The system is already fully initialized when your program runs. Key entry points:

| Address | Routine | Description |
|---------|---------|-------------|
| `$A000` | `Chrout` | Output character (routed by IO_MODE) |
| `$A003` | `Chrin` | Read character from input buffer (non-blocking — C=1 with the character in A, C=0 if none; loop for blocking behaviour) |
| `$A00C` | `BufferSize` | Number of unread bytes in input buffer |
| `$A018` | `VideoClear` | Clear screen and reset cursor |
| `$A01B` | `VideoPutChar` | Write character at cursor position |
| `$A01E` | `VideoSetCursor` | Set cursor position (X=col, Y=row) |
| `$A027` | `VideoSetColor` | Set text color (A = fg<<4 \| bg); on BIOS 2.x the pen for text printed from then on, and the border |
| `$A033` | `SidPlayNote` | Play note (A=voice, X=freqLo, Y=freqHi) |
| `$A075` | `SysDelay` | Delay A=lo, X=hi centiseconds |
| `$A048` | `ReadJoystick1` | Read joystick 1 bitmask |

See `6502.inc` for the complete jump table with calling conventions. On BIOS 2.x the jump table grew by thirteen PICOVDP entries (`VdpInfo` through `VdpStatus`, `$A0B1–$A0D5`): see Section 4 of `6502-VDP.inc`.

### Hardware Detection

Check `HW_PRESENT` (`$030D`) before using optional hardware:

```asm
lda HW_PRESENT
and #HW_SID            ; Is SID present?
beq @NoSound           ; Skip sound code if not
jsr SidPlayNote
@NoSound:
```

## Building for the 6502-PICOVDP

`make VDP=1` builds `Program-VDP.prg` with `6502-VDP.inc` instead of `6502.inc`. `Program.asm` picks the include with the `VDP` symbol, which the Makefile passes to ca65:

```asm
.ifdef VDP
.include "6502-VDP.inc"
.else
.include "6502.inc"
.endif
```

Start from `6502-VDP.inc` when the program needs anything BIOS 2.x adds: the PICOVDP's modes, layers, palette and sprites (`VC_*` register names), or the Kernal's VDP entries. A program built that way needs an ACE converted to a 6502-PICOVDP, running BIOS 2.0 or later, and **does not run on a TMS9918A**. Its build checks `KernalVersion` at `Start` and, on BIOS 1.x, prints `NEEDS BIOS 2 AND A 6502-PICOVDP` and returns to BASIC.

A program built with `6502.inc` that only calls the jump table needs no VDP build: it runs on 2.x as it is.

What is different on BIOS 2.x:

- **There is no Monitor.** Load with `LOAD` (see [Loading & Running](#loading--running)); `SYS` passes registers, and a `BRK` prints the registers and returns to BASIC.
- **The font is in the card.** A program that overwrote the pattern table or changed modes gets the text console back with `InitVideo`, which returns after the next vertical blank. BASIC also restores it when a program stops, if the mode was changed through the Kernal's VDP entries (which keep `VID_MODE`); a program that writes the registers itself calls `InitVideo` before it returns.
- **`VideoSetColor` sets the pen**, the colour of text printed from then on, and the border follows the background.

## Building

### Prerequisites

#### CC65 Compiler

On macOS, install via Homebrew:
```bash
brew install cc65
```

For other platforms, see the [cc65 project](https://github.com/cc65/cc65).

#### bin2woz (optional — for Wozmon loading)

```bash
npm install -g bin2woz
```

Converts the binary to a format loadable via the Wozmon serial monitor. See the [bin2woz project](https://github.com/acwright/bin2woz).

#### cffs (optional — for CompactFlash images)

```bash
npm install -g cffs-image-tool
```

Creates CompactFlash disk images with the program file. See the [cffs project](https://github.com/acwright/cffs).

#### 6502 CLI (optional — for `make run`)

Installed via the [6502-EMULATOR](https://github.com/acwright/6502-EMULATOR) app's Settings → Command Line → Install.

### Build Commands

| Command | Description |
|---------|-------------|
| `make` | Build all targets (`.prg`, `.woz`, and CF image) |
| `make VDP=1` | Build all targets for the 6502-PICOVDP / BIOS 2.x (`Program-VDP.*`) |
| `make build` | Assemble only (`Program.prg`) |
| `make view` | Display hexdump of the built program |
| `make woz` | Create Wozmon-compatible file (`Program.woz`) |
| `make cf` | Create CompactFlash disk image with the program |
| `make run` | Launch the emulator app with the built program loaded (`make VDP=1 run` selects the PICOVDP card; add `ROM=path/to/BIOS.bin` to boot a local BIOS image) |
| `make clean` | Remove build artifacts |

### Build Output

```bash
make
```

Produces:
- `Program.prg` — Raw binary, load address `$0800`
- `Program.woz` — Wozmon-compatible format for serial upload
- `Program.lst` — Assembly listing file for debugging
- `Program.img` — CompactFlash disk image with the program

`make VDP=1` writes the same four files named `Program-VDP.*`. The file inside `Program-VDP.img` is still `PROGRAM.PRG`.

### Loading & Running

Use BASIC's `LOAD`. Requires BIOS v1.3 or later; the VDP build requires BIOS v2.0 or later.

**From CompactFlash** — the usual case:
```
LOAD "PROGRAM.PRG"
RUN
```

**Over serial**, with no CF card:
```
LOAD
```
Then send `Program.prg` from the host with any terminal that speaks XMODEM
(128-byte blocks, checksum mode). The transfer is padded up to a block boundary,
which is harmless.

Either way, `RUN` executes the stub and `SYS 2060` enters your code at `$080C`.

**From the Monitor** (BIOS 1.x only; 2.x has no Monitor), if you are already
there:
```
L "PROGRAM.PRG"
X
```
`L` loads to `$0800` by default and `X` returns to BASIC, where `RUN` works as
usual.

#### Why the loader matters

Your machine code lives past the end-of-program marker at `$080A`, where BASIC
would otherwise put its variables. Keeping the two apart depends on the loader
telling BASIC how many bytes it wrote, so BASIC can place `VARTAB` past the whole
image rather than at the end of the tokenized line chain.

`LOAD` and the 1.x Monitor's `L` both do this as of BIOS v1.3, and `LOAD`
still does on 2.x. **Wozmon does not** —
it writes bytes one at a time with no notion of a length, so BASIC falls back to
walking the line chain, `VARTAB` lands at `$080C` on top of your code, and the
first variable assignment destroys it. Use Wozmon for code you will enter from
Wozmon itself (or, on 1.x, from the Monitor), not for programs you intend to
`RUN`.

On BIOS versions before v1.3 the byte count was discarded on every path, so all
three loaders had this problem.

## Template Structure

| File | Purpose |
|------|---------|
| `Program.asm` | Main source — BASIC stub, entry point, example code |
| `6502.inc` | System include file for BIOS 1.x and the TMS9918A — Kernal jump table, hardware registers, constants |
| `6502-VDP.inc` | System include file for BIOS 2.x and the 6502-PICOVDP (identical to 6502-ASM's) |
| `6502.cfg` | Linker configuration — memory layout for RAM programs |
| `Makefile` | Build system |

## Customizing

1. Edit `Program.asm` — replace the example code after the `Start:` label with your program
2. Do **not** modify the `BasicStartup` bytes — they must remain at `$0800` for BASIC `RUN` to work
3. Return to BASIC with `RTS` when your program finishes
4. Add additional `.asm` files and `.include` them as needed
5. You have ~30 KB of RAM (`$080C–$7FFF`) for code and data

## Related

- [6502-ACE](https://github.com/acwright/6502-ACE) — the hardware, and the index of the whole family
- [6502-BIOS](https://github.com/acwright/6502-BIOS) — the firmware behind the Kernal jump table: `6502.inc` is its 1.6 API, `6502-VDP.inc` its 2.x API
- [6502-PICOVDP](https://github.com/acwright/6502-PICOVDP) — the video card `6502-VDP.inc` is for
- [6502-EMULATOR](https://github.com/acwright/6502-EMULATOR) — run a program without hardware (`make run`)
- [6502-CRT](https://github.com/acwright/6502-CRT) — the same idea for cartridge ROMs
- [6502-ASM](https://github.com/acwright/6502-ASM) — worked assembly examples
- [6502-DOCS](https://github.com/acwright/6502-DOCS) — the documentation site: the cross-development and assembly guides, and the printable reference cards
- [cffs](https://github.com/acwright/cffs) / [bin2woz](https://github.com/acwright/bin2woz) — the tools behind `make cf` and `make woz`

## License

MIT License — see [LICENSE](LICENSE).
