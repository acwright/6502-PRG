6502-PRG
========

A 6502 assembly language program template for [A.C. Wright 6502 project](https://github.com/acwright/6502).

## BASIC Startup

This section contains a tokenized BASIC program that serves as a loader for the machine code. This should be included at the start of program files. When executed by the BASIC interpreter, it decodes to the program `10 SYS 2060`, which uses the SYS command to jump directly to the machine code entry point at address $080C (decimal 2060). This allows the 6502 assembly program to run directly from BASIC without manual memory address entry.

| Address | Byte | Meaning |
|---------|------|---------|
| `$0800` | `$0A` | Next-line ptr lo → `$080A` |
| `$0801` | `$08` | Next-line ptr hi |
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

## Building Program

To build the program, navigate to its directory and use `make`.

### Prerequisites

#### CC65 Compiler

On macOS, install via Homebrew:
```bash
brew install cc65
```

For other platforms or installation methods, refer to the [cc65 project](https://github.com/cc65/cc65).

#### bin2woz

Install from NPM (recommended):
```bash
npm install -g bin2woz
```

Or build from source:
1. Clone the repository:
   ```bash
   git clone https://github.com/acwright/bin2woz.git
   cd bin2woz
   ```

2. Install dependencies and build:
   ```bash
   npm install
   npm run build
   ```

3. Link globally (optional):
   ```bash
   npm link
   ```

For more information, see the [bin2woz project](https://github.com/acwright/bin2woz).

#### cffs

Install from NPM:
```bash
npm install -g cffs-image-tool
```

The `cffs` tool is used to create CompactFlash disk images and add files to them. It's required for the `make cf` target.

For more information, see the [cffs project](https://github.com/acwright/cffs).

### Available Targets

- `make` or `make all` - Build the program
- `make view` - Display hexdump of the built program
- `make woz` - Create a Wozmon compatible file using [bin2woz](https://github.com/acwright/bin2woz)
- `make clean` - Remove build artifacts

### Example

```bash
cd <directory-name>
make        # Build the program
make view   # View the hexdump
make woz    # Create a Wozmon compatible file
```

