6502-PRG
========

A 6502 assembly language program template for the [A.C. Wright 6502](https://github.com/acwright/6502-ACE) family of computer systems.

## Overview

Programs for this system are loaded into RAM at `$0800` and executed from BASIC. Unlike cartridges (which replace ROM), programs run entirely in RAM alongside the BIOS, Kernal, BASIC interpreter, and Monitor — all of which remain available.

### How It Works

1. The program is loaded into RAM at `$0800` (via `LOAD` from CompactFlash, serial transfer, or Wozmon)
2. BASIC's `RUN` command executes the tokenized stub at the start of the file
3. The stub decodes to `10 SYS 2060`, which calls the machine code entry point at `$080C`
4. Your program runs with full access to the Kernal jump table
5. Return to BASIC with `RTS`

### Memory Layout

| Range | Contents |
|-------|----------|
| `$0000–$0035` | Zero page — system pointers (see `6502.inc` for details) |
| `$0036–$00FF` | Zero page — **free for user programs** (202 bytes) |
| `$0100–$01FF` | CPU stack |
| `$0200–$02FF` | Input ring buffer (managed by Kernal) |
| `$0300–$03FF` | Kernal variables (vectors, cursor, HW flags, etc.) |
| `$0400–$07FF` | User/BASIC variables |
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
| `$0804` | `$9E` | `TOK_SYS` |
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
| `$A003` | `Chrin` | Read character from input buffer (blocking) |
| `$A00C` | `BufferSize` | Number of unread bytes in input buffer |
| `$A018` | `VideoClear` | Clear screen and reset cursor |
| `$A01B` | `VideoPutChar` | Write character at cursor position |
| `$A01E` | `VideoSetCursor` | Set cursor position (X=col, Y=row) |
| `$A027` | `VideoSetColor` | Set text color (A = fg<<4 \| bg) |
| `$A033` | `SidPlayNote` | Play note (A=voice, X=freqLo, Y=freqHi) |
| `$A075` | `SysDelay` | Delay A=lo, X=hi centiseconds |
| `$A048` | `ReadJoystick1` | Read joystick 1 bitmask |

See `6502.inc` for the complete jump table with calling conventions.

### Hardware Detection

Check `HW_PRESENT` (`$030D`) before using optional hardware:

```asm
lda HW_PRESENT
and #HW_SID            ; Is SID present?
beq @NoSound           ; Skip sound code if not
jsr SidPlayNote
@NoSound:
```

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

### Build Commands

| Command | Description |
|---------|-------------|
| `make` | Build all targets (`.prg`, `.woz`, and CF image) |
| `make build` | Assemble only (`Program.prg`) |
| `make view` | Display hexdump of the built program |
| `make woz` | Create Wozmon-compatible file (`Program.woz`) |
| `make cf` | Create CompactFlash disk image with the program |
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

### Loading & Running

**From BASIC (CompactFlash):**
```
LOAD "PROGRAM.PRG"
RUN
```

**From Wozmon (serial):**
Upload `Program.woz` via the serial port at 19200 baud.

**From BASIC (serial):**
```
LOAD
```
Then send `Program.prg` from the host using the raw binary serial protocol (2-byte size header + data).

## Template Structure

| File | Purpose |
|------|---------|
| `Program.asm` | Main source — BASIC stub, entry point, example code |
| `6502.inc` | System include file — Kernal jump table, hardware registers, constants |
| `6502.cfg` | Linker configuration — memory layout for RAM programs |
| `Makefile` | Build system |

## Customizing

1. Edit `Program.asm` — replace the example code after the `Start:` label with your program
2. Do **not** modify the `BasicStartup` bytes — they must remain at `$0800` for BASIC `RUN` to work
3. Return to BASIC with `RTS` when your program finishes
4. Add additional `.asm` files and `.include` them as needed
5. You have ~30 KB of RAM (`$080C–$7FFF`) for code and data

