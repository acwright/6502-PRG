; =============================================================================
;   6502-VDP.inc — the AC6502 ACE with a 6502-PICOVDP
; =============================================================================
;
;   CPU:       WDC 65C02S
;   Video:     6502-PICOVDP (SPEC draft 0.5)
;   BIOS:      v2.0
;   Assembler: ca65 (cc65 toolchain) — uses := for absolute symbols
;
;   Include this file in programs for an ACE whose video card is a
;   6502-PICOVDP, running BIOS 2.x, to access Kernal routines, IO hardware
;   registers, system variables, and useful constants.  It names the
;   PICOVDP's second port pair, its registers, status registers and bit
;   fields as VC_* equates.
;
;   An ACE with a TMS9918A uses 6502.inc instead.  A program built with this
;   file assembles and links for either machine but only runs on this one.
;   To build one source both ways, select the include with ca65's VDP symbol
;   (make VDP=1 passes --asm-define VDP):
;
;     .ifdef VDP
;     .include "../6502-VDP.inc"
;     .else
;     .include "../6502.inc"
;     .endif
;
;   Do not include both: every shared name would be defined twice.
;
;   NAMES
;   -----
;   A name for something the 6502-PICOVDP SPEC defines (a register, a bit
;   field, a capability bit, a font ID, a status value) is VC_*, as in
;   VC_REG_FONT and VC_CAP_FONT.  A name for something the BIOS defines (a
;   Kernal entry, a RAM variable, an HW_* flag) is BIOS.inc's own, as in
;   VdpLoadFont and VDP_CAPS.  BIOS.inc's VDP_* names for SPEC values are
;   internal to the BIOS and are not repeated here, so VDP_* in this file is
;   only ever a Kernal RAM variable.  tools/check-include.py enforces that.
;
;   MEMORY MAP
;   ----------
;   $0000-$0003   Zero Page: System pointers (see below)
;   $0004-$0023   Zero Page: Reserved by BASIC interpreter
;   $0024-$0039   Zero Page: System scratch (CF, serial, delay, XModem)
;   $003A-$00FF   Zero Page: Free for a program that has taken the machine over
;                 (not underneath a running BASIC, which uses it as it interprets)
;   $0100-$01FF   CPU Stack
;   $0200-$02FF   Input Buffer (ring buffer, managed by Kernal)
;   $0300-$03FF   Kernal Variables (vectors, cursor, HW flags, video, etc.)
;   $0400-$04FF   BAS_LINBUF: BASIC's raw input line (NOT user memory, despite
;                 BIOS.inc naming $0400 USER_VARS -- a legacy name kept only
;                 because Wozmon builds its input buffer there)
;   $0500-$05FF   BAS_TOKBUF: BASIC's tokenizing scratch
;   $0600-$07FF   FS sector buffer (clobbered by any FS/St routine call)
;   $0800-$7FFF   Program RAM (30 KB — load address for user code)
;   $8000-$83FF   IO 1: Banked RAM Low  (256 × ~1 KB banks)
;   $8400-$87FF   IO 2: Banked RAM High (256 × ~1 KB banks)
;   $8800-$881F   IO 3: RTC (DS1511Y)
;   $8C00-$8C07   IO 4: Storage (CompactFlash, 8-bit IDE)
;   $9000-$9003   IO 5: Serial (R65C51 ACIA)
;   $9400-$940F   IO 6: GPIO (65C22 VIA — keyboard & joysticks)
;   $9800-$981C   IO 7: Sound (SID / ARMSID)
;   $9C00-$9C03   IO 8: Video (6502-PICOVDP, two port pairs)
;   $A000-$A0FF   Kernal Jump Table (stable entry points)
;   $A100-$BFFF   Kernal Implementation
;   $C000-$FEFF   BASIC Interpreter
;   $FF00-$FFF9   Wozmon (mini monitor; SYS 65280 from BASIC)
;   $FFFA-$FFFF   CPU Vectors (NMI, RESET, IRQ)
;
;   There is no machine-code Monitor and no character set in ROM on 2.x.
;   The text font is the card's own (font $00), loaded by InitVideo.
;
;   BOOT SEQUENCE
;   -------------
;   On RESET the BIOS probes all eight IO slots, initializes detected
;   hardware, and auto-selects video or serial console.  If neither is
;   found, IO_MODE is left unchanged (no halt — cartridges with their
;   own display hardware can still boot).  A startup beep plays (if SID
;   is present) for audible feedback, then BOOT_VECTOR ($035B) is
;   checked — if non-zero, execution jumps there (cartridge takeover).
;   Otherwise a console check halts if no video/serial is found, and the
;   machine goes straight to BASIC, which prints its header:
;     AC6502 BIOS v2.0
;     BASIC v2.0 NNNNN BYTES FREE
;   On video the header is the console's first use, which brings the Text
;   console up (see VIDEO CONSOLE).
;
;   VIDEO CONSOLE
;   -------------
;   The probe sets HW_VID only for a 6502-PICOVDP (STAT4 = VC_ID) whose STAT6
;   has VC_CAP_FONT set; it records STAT5 in VDP_FW and STAT6 in VDP_CAPS.
;   STAT5 gates nothing.  KernalInit leaves the card as its own reset does:
;   the legacy submode (VMODE $0), unscrolled.  The first Chrout,
;   VideoClear, VideoSetCursor, VideoGetCursor or VideoSetColor brings up the
;   40×24 Text console (InitVideo, then a clear).  So a cartridge written for
;   1.x runs unchanged: one that programs M1/M2/M3 and its tables finds the
;   card as a TMS9918 was left, and one that prints gets the Text console.
;   A program never needs to check STAT4 or switch modes to print.
;
;   Every character is written with an attribute byte, fg<<4 | bg, from the
;   pen VID_PEN.  The console scrolls in hardware (L0SCRY = VID_TOP × 8).
;   The Kernal drives the card through port A only, and every entry assumes
;   and leaves VBANK = 0 and VINC = +1.
;
;   PORT B FOR INTERRUPT HANDLERS
;   -----------------------------
;   The Kernal never touches port B ($9C02/$9C03), so an interrupt handler
;   that uses it cannot land between the two writes of a foreground command,
;   and needs no sei/cli around foreground VDP work.  A handler acknowledges
;   through STAT1 (VC_REG_STATSEL_B = VC_STAT1), which clears the interrupt
;   latches and leaves STAT0's flags for foreground code.  A handler that
;   writes VBANK or VINC puts them back: both ports share them.  InitVideo
;   writes IRQEN = 0, so re-enable the interrupt after anything that calls
;   it (the console's first use, BASIC's SCREEN 0 or return to text).
;
;   GRACEFUL DEGRADATION
;   --------------------
;   Every Kernal routine that drives a card checks HW_PRESENT first, so
;   a call on a machine without that card is safe rather than a crash.
;   Routines with nothing to report back return silently (video console and
;   SID); routines that move data report failure in the carry (CompactFlash,
;   the NVRAM save slots, and the PICOVDP entries VdpInfo-VdpStatus, which
;   write nothing with no card; WaitVBlank waits 2 cs instead).  An empty
;   slot, a TMS9918A, and a PICOVDP without the built-in font all count as
;   no card.  The raw NVRAM pair RtcReadNVRAM / RtcWriteNVRAM is the
;   exception: it does not check, so test HW_RTC before calling it.
;   The per-routine notes below say which behaviour each one has.
;
;   CARTRIDGE SUPPORT
;   -----------------
;   Cartridges overlay $C000-$FFFF (BASIC, Wozmon, vectors).  The Kernal
;   ($A000-$BFFF, all of it on 2.x) remains accessible.  Use KernalInit
;   ($A078) to initialize all hardware and return via RTS — interrupts are
;   left disabled so the cart can set up its own handlers first.  BRK_PTR
;   points at the Kernal's BRK report, which warm-starts BASIC at $C000; a
;   cartridge must point it at its own handler.  See usage patterns in the
;   BIOS README.
;
;   IO_MODE
;   -------
;   Bit 0 of IO_MODE selects the console output device:
;     0 = Video (6502-PICOVDP)    1 = Serial (R65C51)
;   Chrout ($A000) dispatches automatically based on IO_MODE.
;   Use SetIOMode / GetIOMode to change or query the active console.
;
;   CALLING CONVENTION
;   ------------------
;   Kernal routines are called via JSR to the jump table addresses below.
;   Parameters are passed in A, X, Y, and/or zero-page pointers as
;   documented per routine.  Carry flag is used for success/error returns
;   (C=0 success, C=1 error) where noted.
;
;   HW_PRESENT BITMASK
;   -------------------
;   After boot, HW_PRESENT ($030D) contains a bitmask of detected hardware.
;   Test individual bits before calling hardware-specific routines.
;   Example:
;     lda HW_PRESENT
;     and #HW_VID          ; Is a PICOVDP with the built-in font present?
;     beq @NoVideo         ; Skip if not
;
; =============================================================================


; =============================================================================
;   SECTION 1: ZERO PAGE POINTERS
; =============================================================================
; These zero page locations are used by the Kernal.  External programs may
; use STR_PTR and CF_BUF_PTR/CF_LBA as parameters to Kernal calls.
; Locations $3A-$FF are free for user programs.

STR_PTR             := $02               ; 2 bytes — String pointer (used by FsLoadFile, FsSaveFile, FsDeleteFile, etc.)
CF_BUF_PTR          := $24               ; 2 bytes — CF sector data buffer pointer
CF_LBA              := $26               ; 4 bytes — CF 28-bit LBA address (little-endian)
XFER_PTR            := $2A               ; 2 bytes — XModem transfer data pointer (source for XModemSave, dest for XModemLoad)
NV_PTR              := STR_PTR           ; 2 bytes — Caller's buffer during an NvRead / NvWrite copy (clobbered by both)

; Safe zero page range for user programs:
; $3A-$FF (198 bytes free)
;   $2C-$2D belong to SysDelay and $36-$39 to XModemLoad/XModemSave; neither
;   survives a call into those routines.  $2E-$35 are unassigned on 2.x (1.x's
;   Monitor used them)


; =============================================================================
;   SECTION 2: SYSTEM VARIABLES ($0300-$03FF)
; =============================================================================

IRQ_PTR             := $0300             ; 2 bytes — IRQ handler vector (writable; default: internal Irq handler)
BRK_PTR             := $0302             ; 2 bytes — BRK handler vector (writable; default: the BRK report, then BASIC)
NMI_PTR             := $0304             ; 2 bytes — NMI handler vector (writable; default: RTI)

IO_MODE             := $0306             ; 1 byte  — Console output mode (bit 0: 0=video, 1=serial)
VID_CURSOR_X        := $0307             ; 1 byte  — Video cursor column (0-39)
VID_CURSOR_Y        := $0308             ; 1 byte  — Video cursor row (0-23)

HW_PRESENT          := $030D             ; 1 byte  — Hardware present bitmask (set during boot probe)
CF_DISK             := $030F             ; 1 byte  — Current CF disk bank (0–255); base LBA = CF_DISK × FS_DISK_SECTORS; reset to 0 on boot

; HW_PRESENT bit definitions (bit order matches IO slot numbers):
HW_RAM_L            = %00000001          ; Bit 0: RAM card low  (IO 1, $8000-$83FF)
HW_RAM_H            = %00000010          ; Bit 1: RAM card high (IO 2, $8400-$87FF)
HW_RTC              = %00000100          ; Bit 2: RTC card      (IO 3, DS1511Y)
HW_CF               = %00001000          ; Bit 3: Storage card   (IO 4, CompactFlash)
HW_SC               = %00010000          ; Bit 4: Serial card    (IO 5, R65C51)
HW_GPIO             = %00100000          ; Bit 5: GPIO card      (IO 6, 65C22 VIA — keyboard/joysticks)
HW_SID              = %01000000          ; Bit 6: Sound card     (IO 7, SID/ARMSID)
HW_VID              = %10000000          ; Bit 7: Video card     (IO 8, a 6502-PICOVDP with the built-in font)

; BRK register save area.  The BRK handler fills it and prints a report:
;     BREAK $nn AT $xxxx
;     A=xx X=xx Y=xx P=xx S=xx
; ($xxxx the BRK opcode, $nn the byte after it, S the stack pointer before the
; BRK), then warm-starts BASIC with the program kept.  CONT cannot continue.
; BASIC's SYS addr[,a[,x[,y]]] also leaves the routine's returned A, X, Y and P
; in BRK_A, BRK_X, BRK_Y and BRK_P.
BRK_P               := $0310             ; Saved processor status (P)
BRK_PCL             := $0311             ; Saved PC low byte (points to BRK instruction + 2)
BRK_PCH             := $0312             ; Saved PC high byte
BRK_A               := $0313             ; Saved accumulator
BRK_X               := $0314             ; Saved X register
BRK_Y               := $0315             ; Saved Y register
BRK_SP              := $0316             ; Saved stack pointer

XFER_REMAIN         := $0317             ; 2 bytes — XModem byte count (output from XModemLoad; input to XModemSave)
XFER_IO_SAVE        := $0319             ; 1 byte  — Saved IO_MODE during XModem transfer (internal use)

; RTC scratch (used by RtcWriteDate to pass century):
RTC_BUF_CENT        := $030B             ; Century value for RtcReadDate / RtcWriteDate

; Cartridge / boot redirect vector:
BOOT_VECTOR         := $035B             ; 2 bytes — Boot redirect address (0=normal boot)
                                         ;   Set after KernalInit to redirect Reset flow
                                         ;   KernalInit zeroes this; write after init returns
FS_IO_ADDR          := $037F             ; 2 bytes — Load/save target address for FsLoadFileAddr / FsSaveFileAddr
FS_FILE_SIZE        := $034A             ; 2 bytes — File size in bytes (set by caller for FsSaveFileAddr; populated by FsLoadFileAddr)

; Program image handoff (v1.3):
PRG_IMAGE_END       := $038E             ; 2 bytes — End address of a program image placed at
                                         ;   PROGRAM_START by a loader running outside BASIC
                                         ;   (e.g. a cartridge's own loader); 0 = none.
                                         ;   KernalInit zeroes it; BASIC consumes and re-zeroes it
                                         ;   on entry, so a stale value is never acted on twice.
                                         ;   A loader that drops a .prg at $0800 and then hands
                                         ;   off to BASIC should write the end address here so
                                         ;   the attached machine code survives LOAD/SAVE.

; NVRAM save slots (v1.6):
NV_ID               := $0390             ; 1 byte  — Owner ID input for NvWrite ($01-$FF; $00 is refused — that is NvErase)

; Video variables (v2.0):
VID_PEN             := $0391             ; 1 byte  — Attribute byte for new output, fg<<4 | bg
                                         ;   KernalInit sets $1F (black on white); NEW, RUN,
                                         ;   CLR and errors keep it.  Set it with VideoSetColor
VID_TOP             := $0392             ; 1 byte  — Name-table row shown at the top (0-23);
                                         ;   L0SCRY = VID_TOP × 8.  A cell's name byte is at
                                         ;   ((row + VID_TOP) mod 24) × 40 + column
VID_MODE            := $0393             ; 1 byte  — $00 = console not set up since KernalInit
                                         ;   (card in the legacy submode); $01 = Text console
                                         ;   intact; otherwise low nibble = the VMODE the Kernal
                                         ;   last set, b7 = disturbed (sprites, scroll, layers)
VDP_FW              := $0394             ; 1 byte  — STAT5 (firmware version, BCD) at boot; $00 if no PICOVDP
VDP_CAPS            := $0395             ; 1 byte  — STAT6 (capability bits, VC_CAP_*) at boot; $00 if no PICOVDP
VDP_L0CTRL_SHADOW   := $0396             ; 1 byte  — What was last written to the write-only L0CTRL
VDP_L1CTRL_SHADOW   := $0397             ; 1 byte  — What was last written to the write-only L1CTRL
                                         ;   Both are kept by VdpWriteReg, VdpSetScroll, VdpLayer
                                         ;   and InitVideo; a direct register write bypasses them
VDP_P0              := $0398             ; 1 byte  — Parameter block for VdpSprite and VdpSetScroll
VDP_P1              := $0399             ; 1 byte
VDP_P2              := $039A             ; 1 byte
VDP_P3              := $039B             ; 1 byte


; =============================================================================
;   SECTION 3: KEY MEMORY LOCATIONS
; =============================================================================

INPUT_BUFFER        := $0200             ; $0200-$02FF — 256-byte ring buffer (keyboard/serial input)
PROGRAM_START       := $0800             ; User programs and BASIC programs load here
;                                        ; Program space extends to $7FFF (30 KB)
MEMORY_TOP          := $8000             ; First address past usable RAM (IO space starts here)


; =============================================================================
;   SECTION 4: KERNAL JUMP TABLE ($A000-$A0FF)
; =============================================================================
; All entry points are stable 3-byte JMP instructions.  Call via JSR.
; 72 entries ($A000-$A0D5); the reserved slots after them ($A0D8-$A0FE) return
; immediately (RTS).

; --- Character I/O ---
Chrout              := $A000             ; Output character (dispatched by IO_MODE)
                                         ;   In: A = character
                                         ;   Preserves: A, X, Y
                                         ;   Handles CR ($0D), LF ($0A), BS ($08), BEL ($07) in video mode

Chrin               := $A003             ; Input character from buffer (non-blocking poll)
                                         ;   Out: A = character, C=1 if char available, C=0 if not
                                         ;   Caller must loop until C=1 for blocking behavior
                                         ;   Echoes character via Chrout
                                         ;   Modifies: A

WriteBuffer         := $A006             ; Write byte to input ring buffer
                                         ;   In: A = byte to write
                                         ;   Modifies: X

ReadBuffer          := $A009             ; Read byte from input ring buffer
                                         ;   Out: A = byte read
                                         ;   Modifies: X, A

BufferSize          := $A00C             ; Get number of unread bytes in input buffer
                                         ;   Out: A = count
                                         ;   Modifies: A

; --- IO Mode ---
SetIOMode           := $A00F             ; Set IO_MODE
                                         ;   In: A = mode (bit 0: 0=video, 1=serial)

GetIOMode           := $A012             ; Get IO_MODE
                                         ;   Out: A = current IO_MODE

; --- Video console (6502-PICOVDP) ---
;   The console entries marked "brings the console up" call InitVideo and
;   clear the screen first if nothing has since KernalInit (VID_MODE = $00).
InitVideo           := $A015             ; Put the PICOVDP in the Kernal's Text-mode console
                                         ;   With the display off, writes the Text layout: VMODE
                                         ;   Text (40×24 of 6×8), name table $0000, attributes
                                         ;   $0400, pattern table $0800, PALBASE $3F ($FC00),
                                         ;   VBANK 0, VINC +1, IRQEN 0, STATSEL_A 0, scroll 0,
                                         ;   layer 1 and sprites off.  Then has the card load its
                                         ;   built-in font into $0800 (VdpLoadFont, font $00) and
                                         ;   waits for it: returns after the next vertical blank,
                                         ;   up to a frame later.  Restores palette row 0, sets
                                         ;   the border from VID_PEN, turns the display on and
                                         ;   sets VID_MODE = $01
                                         ;   Does not clear the screen: the name and attribute
                                         ;   tables are shown as they are, unscrolled
                                         ;   A program that switched modes, moved tables or
                                         ;   overwrote the glyphs gets the console back with this
                                         ;   one call.  It writes IRQEN = 0, so a program using a
                                         ;   VDP interrupt enables it again afterwards
                                         ;   Skips silently if no video card is fitted
                                         ;   Modifies: Flags, A, X, Y

VideoClear          := $A018             ; Clear screen to spaces in the pen, cursor home, unscrolled
                                         ;   Fills the 960 names at $0000 with $20 and the 960
                                         ;   attributes at $0400 with VID_PEN; VID_TOP = 0
                                         ;   Brings the console up
                                         ;   Skips silently if no video card is fitted
                                         ;   Modifies: Flags, A, X, Y

VideoPutChar        := $A01B             ; Write character at cursor position (no cursor advance)
                                         ;   In: A = character; its attribute byte is VID_PEN
                                         ;   No card check and no console bring-up
                                         ;   Modifies: Flags

VideoSetCursor      := $A01E             ; Set cursor position
                                         ;   In: X = column (0-39), Y = row (0-23), on the screen
                                         ;   as shown (VID_TOP is folded in)
                                         ;   Brings the console up
                                         ;   Skips silently if no video card is fitted
                                         ;   Modifies: Flags, A

VideoGetCursor      := $A021             ; Get cursor position
                                         ;   Out: X = column (0-39), Y = row (0-23), screen coordinates
                                         ;   Brings the console up
                                         ;   Modifies: Flags

VideoScroll         := $A024             ; Scroll screen up one line, in hardware
                                         ;   Moves the origin down a row (VID_TOP, L0SCRY) and
                                         ;   blanks the new bottom row in the pen; nothing is
                                         ;   copied.  The cursor keeps its screen position
                                         ;   Skips silently if no video card is fitted
                                         ;   Modifies: Flags, A, X, Y

VideoSetColor       := $A027             ; Set the pen for later output, and the border
                                         ;   In: A = fg<<4 | bg (palette row 0, see Colour Constants)
                                         ;   VID_PEN = A, and register 7 (VC_REG_COLOR) = A: its
                                         ;   low nibble is the border, so the border follows the
                                         ;   background.  Characters already on screen keep
                                         ;   their colours; VideoClear fills with the pen
                                         ;   Brings the console up
                                         ;   Skips silently if no video card is fitted
                                         ;   Modifies: Flags, A

VideoChroutRaw      := $A02A             ; Output char to video with no control-code handling
                                         ;   In: A = character code (0-255), glyph n of the font
                                         ;   Preserves: A, X, Y
                                         ;   Always renders glyph, never interprets CR/LF/BS/BEL
                                         ;   Brings the console up

; --- Sound (SID / ARMSID) ---
InitSID             := $A02D             ; Initialize SID (clear all registers, volume=15)
                                         ;   Modifies: A, X

Beep                := $A030             ; Play short beep tone (~475 Hz)
                                         ;   Skips silently if no SID detected
                                         ;   Modifies: A, X, Y

SidPlayNote         := $A033             ; Play note on a SID voice
                                         ;   In: A = voice (0-2), X = freq low, Y = freq high
                                         ;   Uses triangle waveform, ADSR: A=0 D=9 S=F R=8
                                         ;   Full sustain holds the note flat until gated off
                                         ;   Skips silently if no SID is fitted
                                         ;   Modifies: A

SidSilence          := $A036             ; Silence all 3 SID voices (gate off only)
                                         ;   Frequencies are left intact — the envelope release
                                         ;   rings the tone out.  Zeroing freq would freeze the
                                         ;   waveform and decay a DC offset (audible thump)
                                         ;   Skips silently if no SID is fitted
                                         ;   Modifies: A

SidSetVolume        := $A039             ; Set SID master volume
                                         ;   In: A = volume (0-15); only the low nibble is used
                                         ;   Skips silently if no SID is fitted
                                         ;   Modifies: A

; --- Filesystem (disk-banked, 16-file directory per disk) ---
;   The CF card is divided into 256 disk banks of 1 MB each (FS_DISK_SECTORS
;   sectors × 512 bytes).  CF_DISK ($030F) selects the active bank (reset to
;   0 on boot).  Use FsSetDisk / FsGetDisk to change it at runtime.  Each
;   bank has its own directory at the first sector, holding up to 16 entries
;   (8.3 filenames, space-padded, case-insensitive).  All FS routines and
;   StReadSector / StWriteSector operate on the selected bank automatically.
;   Pass null-terminated "NAME.EXT" string via STR_PTR ($02-$03).
;   Use FsLoadFileAddr / FsSaveFileAddr for load-to-address transfers.
;   The extension is inert — nothing in the BIOS branches on it, so .PRG, .BAS
;   and .FOO behave identically.  What a file means is decided by the routine
;   that loads it, never by its name.
;   All FS routines use $0600-$07FF as a 512-byte sector buffer.

FsLoadFile          := $A03C             ; Load file from CF
                                         ;   In: STR_PTR = pointer to null-terminated filename
                                         ;   Out: C=0 success, C=1 not found or error
                                         ;   Modifies: A, X, Y

FsSaveFile          := $A03F             ; Save file to CF
                                         ;   In: STR_PTR = pointer to null-terminated filename
                                         ;   Out: C=0 success, C=1 error
                                         ;   Modifies: A, X, Y

FsDeleteFile        := $A042             ; Delete file from CF
                                         ;   In: STR_PTR = pointer to null-terminated filename
                                         ;   Out: C=0 success, C=1 not found or error
                                         ;   Modifies: A, X, Y

; --- Keyboard / GPIO ---
InitKB              := $A045             ; Initialize VIA for keyboard input
                                         ;   Configures Port A/B as inputs, enables CB1/CA1 IRQs
                                         ;   Modifies: A

; --- Joysticks ---
;   Both ports are ACTIVE LOW — each line is pulled up through 1K and grounded
;   by its switch — and both routines return the port read raw.  So a held
;   button is a 0 bit and an untouched stick reads $FF:
;
;     Bit:  7   6   5   4   3   2   1   0
;           R   L   D   U   Y   X   B   A
;
;   Test a direction with:  lda ... / and #%00010000 / beq @UpHeld
;
;   Neither routine checks HW_PRESENT — with no GPIO card fitted they read a
;   floating bus and the value is meaningless.  Test HW_GPIO first if the card
;   may be absent.  (BASIC's JOY() does this and reports $FF.)
;
;   Both routines release both encoders (KBDisable), read the port, then
;   re-enable them (KBEnable).  The interrupt state is left alone.

ReadJoystick1       := $A048             ; Read joystick 1 (VIA Port B)
                                         ;   Out: A = bitmask (active low, see above)
                                         ;   Modifies: Flags, A

ReadJoystick2       := $A04B             ; Read joystick 2 (VIA Port A)
                                         ;   Out: A = bitmask (active low, see above)
                                         ;   Modifies: Flags, A

; --- Serial (R65C51) ---
InitSC              := $A04E             ; Initialize serial card (8-N-1, 19200 baud)
                                         ;   Modifies: A

SerialChrout        := $A051             ; Direct serial output (bypasses IO_MODE)
                                         ;   In: A = character
                                         ;   Blocks until TX register is empty

; --- Serial Transfer ---
XModemLoad          := $A054             ; Receive data via XModem into memory
                                         ;   Protocol: standard XModem (128-byte blocks, checksum)
                                         ;   In: XFER_PTR = destination address
                                         ;   Out: C=0 success, XFER_PTR advanced past last byte written
                                         ;        XFER_REMAIN = total bytes received
                                         ;        C=1 transfer failed (timeout or CAN received)
                                         ;   Modifies: A, X, Y

XModemSave          := $A057             ; Send data via XModem from memory
                                         ;   Protocol: standard XModem (128-byte blocks, checksum)
                                         ;   In: XFER_PTR = source address, XFER_REMAIN = byte count
                                         ;   Out: C=0 success, C=1 transfer failed (timeout or CAN received)
                                         ;   Modifies: A, X, Y

; --- RTC (DS1511Y) ---
RtcReadTime         := $A05A             ; Read current time
                                         ;   Out: A = hours, X = minutes, Y = seconds (all binary)

RtcReadDate         := $A05D             ; Read current date
                                         ;   Out: A = day, X = month, Y = year (all binary)
                                         ;   RTC_BUF_CENT ($030B) = century

RtcWriteTime        := $A060             ; Set RTC time
                                         ;   In: A = hours, X = minutes, Y = seconds (all binary)
                                         ;   Modifies: A, X

RtcWriteDate        := $A063             ; Set RTC date
                                         ;   In: A = day, X = month, Y = year (all binary)
                                         ;   RTC_BUF_CENT ($030B) must hold century
                                         ;   Modifies: A, X

RtcReadNVRAM        := $A066             ; Read byte from DS1511Y NVRAM (256 bytes)
                                         ;   In: X = address ($00-$FF)
                                         ;   Out: A = data byte

RtcWriteNVRAM       := $A069             ; Write byte to DS1511Y NVRAM
                                         ;   In: X = address ($00-$FF), A = data byte

; --- CompactFlash Storage ---
StReadSector        := $A06C             ; Read one 512-byte sector
                                         ;   In: CF_LBA ($26-$29) = LBA, CF_BUF_PTR ($24-$25) = dest
                                         ;   Out: C=0 success, C=1 error; CF_BUF_PTR advanced by 512
                                         ;   Modifies: A, X, Y

StWriteSector       := $A06F             ; Write one 512-byte sector
                                         ;   In: CF_LBA ($26-$29) = LBA, CF_BUF_PTR ($24-$25) = source
                                         ;   Out: C=0 success, C=1 error; CF_BUF_PTR advanced by 512
                                         ;   Modifies: A, X, Y

StWaitReady         := $A072             ; Wait for CF to become ready (BSY=0, RDY=1)
                                         ;   Out: C=0 ready, C=1 timeout/error
                                         ;   Returns C=1 at once if the boot probe found no CF
                                         ;   card, rather than spinning out the full timeout.
                                         ;   Every FS and sector routine passes through here, so
                                         ;   an empty slot fails fast throughout the stack
                                         ;   Modifies: A, X, Y

; --- Timing ---
SysDelay            := $A075             ; Delay for specified centiseconds (10 ms units)
                                         ;   In: A = low byte, X = high byte (16-bit count)
                                         ;   Uses VIA T1 if available, else software loop
                                         ;   Modifies: A, X, Y (X/Y clobbered only in software-fallback path)

; --- System / Cartridge Support ---
KernalInit          := $A078             ; Initialize all hardware (vectors, probe & init all I/O)
                                         ;   Caller MUST reset the stack pointer before JSR:
                                         ;     ldx #$ff / txs / jsr KernalInit
                                         ;   Zeroes BOOT_VECTOR, PRG_IMAGE_END and CF_DISK;
                                         ;   sets HW_PRESENT, IO_MODE, IRQ/BRK/NMI ptrs,
                                         ;   VID_PEN = $1F, VDP_FW, VDP_CAPS and VID_MODE = $00
                                         ;   Leaves a PICOVDP in the legacy submode (VMODE $0),
                                         ;   unscrolled, as its own reset does: the Text console
                                         ;   comes up on first use, not here
                                         ;   Does NOT reset the stack pointer
                                         ;   Does NOT enable interrupts (caller must CLI)
                                         ;   Does NOT play beep, print, or start BASIC
                                         ;   Does NOT halt if no console is detected
                                         ;   Modifies: All registers, flags

KernalVersion       := $A07B             ; Get BIOS version
                                         ;   Out: A = major version, X = minor version

; --- Disk-banking & addressed storage (v1.1) ---
;   CF_DISK ($030F) selects the current disk bank (0–255).
;   All FS routines, StReadSector, and StWriteSector address the selected
;   bank automatically.  Bank changes survive until the next reset.

FsLoadFileAddr      := $A07E             ; Load named file from current disk to FS_IO_ADDR
                                         ;   In: STR_PTR = null-terminated filename
                                         ;       FS_IO_ADDR ($037F) = destination address
                                         ;   Out: C=0 success, FS_FILE_SIZE = bytes loaded; C=1 error
                                         ;   Modifies: A, X, Y

FsSaveFileAddr      := $A081             ; Save FS_FILE_SIZE bytes from FS_IO_ADDR to a named file
                                         ;   In: STR_PTR = filename
                                         ;       FS_IO_ADDR ($037F) = source address
                                         ;       FS_FILE_SIZE ($034A) = byte count
                                         ;   Out: C=0 success, C=1 error (disk full or write error)
                                         ;   Modifies: A, X, Y

FsFormatDisk        := $A084             ; Zero the current disk's directory (erases all 16 entries)
                                         ;   Out: C=0 success, C=1 write error
                                         ;   No confirmation prompt — caller is responsible
                                         ;   Modifies: A, X, Y

FsSetDisk           := $A087             ; Select the current CF disk bank
                                         ;   In: A = disk number (0–255) → stored in CF_DISK ($030F)
                                         ;   Modifies: Flags

FsGetDisk           := $A08A             ; Get the current CF disk bank
                                         ;   Out: A = CF_DISK value (0–255)
                                         ;   Modifies: Flags, A

FsPrintDisk         := $A08D             ; Print "DISK n" + CRLF via Chrout (decimal, no leading zeros)
                                         ;   Uses current CF_DISK
                                         ;   Modifies: Flags, A, X, Y

; --- General Output Utilities (console routed by IO_MODE via Chrout) ---
PrintStr            := $A090             ; Print a NUL-terminated string
                                         ;   In : A = string addr low, Y = string addr high
                                         ;   Modifies: Flags, A, Y, STR_PTR (X preserved)
PrintCRLF           := $A093             ; Print CR ($0D) then LF ($0A)
                                         ;   Modifies: Flags, A
PrintDecU16         := $A096             ; Print an unsigned 16-bit value as decimal
                                         ;   In : A = value low, X = value high (no leading zeros)
                                         ;   Modifies: Flags, A, X, Y, FS_FILE_SIZE, FS_DIR_IDX

; --- Keyboard Encoder Control (raw GPIO port access) ---
;   Both keyboard encoders drive the VIA ports the joysticks share.  Disable
;   them to read GPIO_PORTA/GPIO_PORTB directly, then re-enable them.  The
;   ReadJoystick routines do this for you; call these only for custom reads.
KBDisable           := $A099             ; Release both encoders and wait for them to let go
                                         ;   Busy-waits ~200 us so the ports settle before reading
                                         ;   Safe to call while the caller owns the VIA timers
                                         ;   Modifies: Flags, A
KBEnable            := $A09C             ; Re-enable both keyboard encoders
                                         ;   Modifies: Flags, A

; --- NVRAM Save Slots (DS1511Y) (v1.6) ---
;   The 256 NVRAM bytes as 16 slots.  Slot n is NVRAM n*16 .. n*16+15:
;     +$0      owner ID ($00 = free, otherwise a byte the game chooses)
;     +$1      checksum over the owner ID and the 14 payload bytes
;     +$2-$F   payload (NV_SLOT_DATA bytes)
;   Checksum: ck = NV_CK_SEED, then ck = rol8(ck) EOR b for each covered byte
;   (rol8 = shift left, bit 7 back into bit 0).  Each slot is validated alone.
;
;   All six:
;     * Carry set means the call did nothing: no RTC fitted, slot >= NV_SLOTS,
;       or the per-routine cases noted.  They check HW_PRESENT themselves.
;     * The caller's D and I flags come back unchanged.  Interrupts are held
;       off for a slot copy, which uses the DS1511Y's burst mode (every access
;       of RTC_RAM_DATA advances RTC_RAM_ADDR); an NMI handler must not touch
;       NVRAM.  Burst mode is off again on return.
NvStat              := $A09F             ; Report a slot's state
                                         ;   In:  X = slot (0-15)
                                         ;   Out: A = NV_EMPTY / NV_VALID / NV_BAD, Y = owner ID
                                         ;        C set if no RTC or bad slot (A, Y undefined)
                                         ;   Modifies: Flags, A, Y (X preserved)

NvRead              := $A0A2             ; Copy a valid slot's 14 payload bytes
                                         ;   In:  X = slot, A/Y = destination lo/hi
                                         ;   Out: A = status, Y = owner ID
                                         ;        C clear and the buffer written only if A = NV_VALID
                                         ;        C set on a slot that is not valid (A, Y still the
                                         ;        status and owner) — the buffer is never touched
                                         ;   Modifies: Flags, A, Y, NV_PTR (X preserved)

NvWrite             := $A0A5             ; Write a slot: owner ID, checksum and 14 payload bytes
                                         ;   In:  X = slot, A/Y = source lo/hi, NV_ID = owner ID ($01-$FF)
                                         ;   Out: C set if no RTC, bad slot or NV_ID = 0 (nothing written)
                                         ;   Modifies: Flags, A, Y, NV_PTR (X preserved)

NvErase             := $A0A8             ; Zero all 16 bytes of a slot
                                         ;   In:  X = slot
                                         ;   Out: C set if no RTC or bad slot
                                         ;   Modifies: Flags, A, Y (X preserved)

NvFind              := $A0AB             ; Lowest slot whose owner ID is A, valid or damaged
                                         ;   In:  A = owner ID ($00 finds the lowest free slot)
                                         ;   Out: X = slot; C set if no slot matched (X undefined)
                                         ;   Modifies: Flags, A, X

NvFormat            := $A0AE             ; Erase all 16 slots
                                         ;   Out: C set if no RTC
                                         ;   Modifies: Flags, A, X, Y

; --- PICOVDP (v2.0) ---
;   Each returns carry set, having written nothing, when no card is fitted
;   (HW_VID clear) or an argument is out of range, and carry clear otherwise.
;   Port A only, and VBANK = 0, VINC = +1 on the way out, like the console.
;   Only VdpWriteReg and the entries built on it keep VID_MODE and the LxCTRL
;   shadows; a program that writes VC_REG directly keeps its own records.

VdpInfo             := $A0B1             ; What the boot probe found
                                         ;   Out: A = VDP_FW (STAT5), X = VDP_CAPS (STAT6),
                                         ;        Y = VC_ID ($AC), carry clear
                                         ;        No card: carry set, Y = 0, A = X = 0 (or, for a
                                         ;        PICOVDP without the font, what it reported)
                                         ;   Modifies: Flags, A, X, Y

VdpWriteReg         := $A0B4             ; Write a register, keeping the Kernal's records of it
                                         ;   In: A = value, X = register (0-127, VC_REG_*)
                                         ;   A VMODE write sets VID_MODE (b7 set when it takes the
                                         ;   card back to Text or legacy from anything else); an
                                         ;   L0CTRL / L1CTRL write sets VDP_L0CTRL_SHADOW /
                                         ;   VDP_L1CTRL_SHADOW
                                         ;   Out: C set, nothing written, if no card or X > 127
                                         ;   Preserves: X, Y
                                         ;   Modifies: Flags, A

VdpSetMode          := $A0B7             ; Write VMODE, and VID_MODE with it
                                         ;   In: A = VC_VMODE_TEXT, _COMPACT, _GRAPHICS or _FULL (1-4)
                                         ;   The register only: tables, layers and sprites are the
                                         ;   caller's to lay out.  The Text console is InitVideo
                                         ;   Out: C set, nothing written, if no card or A out of range
                                         ;   Modifies: Flags, A, X

VdpPoke             := $A0BA             ; Write one byte anywhere in VRAM
                                         ;   In: A = value, X = address low, Y = address high
                                         ;       ($0000-$FFFF; VBANK is set and put back to 0)
                                         ;   Out: C set, nothing written, if no card
                                         ;   Modifies: Flags, A, X

VdpPeek             := $A0BD             ; Read one byte anywhere in VRAM
                                         ;   In: X = address low, Y = address high ($0000-$FFFF)
                                         ;   Out: A = the byte; C set, nothing read, if no card
                                         ;   Modifies: Flags, A, X

VdpSetPalette       := $A0C0             ; Set one palette entry, at VC_PALETTE + 2 × entry
                                         ;   In: X = entry (0-255), A = %0000RRRR, Y = %GGGGBBBB
                                         ;   Assumes PALBASE = $3F, where InitVideo puts it; a
                                         ;   program that moves PALBASE writes its own
                                         ;   Out: C set, nothing written, if no card
                                         ;   Modifies: Flags, A, X, Y

WaitVBlank          := $A0C3             ; Return at the start of the next vertical blank
                                         ;   Polls STAT3 b0 on port A, then puts STATSEL_A back to
                                         ;   0 without reading STAT0: STAT0's flags and the STAT1
                                         ;   latches are left as they were
                                         ;   No card: waits 2 cs (SysDelay) and returns C set
                                         ;   Modifies: Flags, A, X, Y

VdpLoadFile         := $A0C6             ; Load a named file from the current disk into VRAM
                                         ;   In: STR_PTR = filename, FS_IO_ADDR ($037F) = VRAM
                                         ;       address (any of the 64 KB)
                                         ;   Writes exactly the file's bytes, never the rest of
                                         ;   its last sector
                                         ;   Out: C clear, FS_FILE_SIZE = bytes written
                                         ;        C set if no card (nothing read), not found, or
                                         ;        read error
                                         ;   Modifies: Flags, A, X, Y, CF_LBA, CF_BUF_PTR,
                                         ;             XFER_REMAIN, FS sector buffer

VdpLoadFont         := $A0C9             ; Load a built-in font into layer 0's pattern table
                                         ;   In: A = font ID; VC_FONT_CP437 ($00) is the only one
                                         ;   accepted.  Writes VC_REG_FONT for layer 0, which
                                         ;   copies to L0PAT × $800 as it stands at the call, and
                                         ;   returns at the next vertical blank (WaitVBlank), when
                                         ;   the copy has landed.  STAT0 is not read
                                         ;   Out: C set, nothing written, if no card or A <> $00
                                         ;   Modifies: Flags, A, X, Y

VdpSprite           := $A0CC             ; Write one sprite's four attribute bytes
                                         ;   In: X = sprite (0-63), VDP_P0 = Y, VDP_P1 = X bits
                                         ;       7:0, VDP_P2 = pattern, VDP_P3 = attributes
                                         ;       (VC_ATTR_*: b7 X bit 8, b6 priority, b5:4 flips,
                                         ;       b3:0 sub-palette)
                                         ;   Assumes the sprite attribute table at VRAM $2000
                                         ;   (SPRATTR = $40, where BASIC's SCREEN 1-3 put it); a
                                         ;   program that moves SPRATTR writes sprites with VdpPoke
                                         ;   Sets VID_MODE b7 (disturbed)
                                         ;   Out: C set, nothing written, if no card or X > 63
                                         ;   Preserves: X
                                         ;   Modifies: Flags, A, Y

VdpSetScroll        := $A0CF             ; Scroll a layer
                                         ;   In: X = layer (0-1), A = x bits 7:0, Y = y,
                                         ;       VDP_P0 = x bit 8 (0 or not)
                                         ;   Writes LxSCRX, LxSCRY and LxCTRL b6 (VC_LCTRL_SCRX8),
                                         ;   LxCTRL's other bits from its shadow
                                         ;   Sets VID_MODE b7 (disturbed)
                                         ;   Out: C set, nothing written, if no card or X > 1
                                         ;   Modifies: Flags, A, X

VdpLayer            := $A0D2             ; Show or hide a layer
                                         ;   In: X = layer (0-1), A = 0 to hide, anything else to show
                                         ;   Sets or clears LxCTRL b4 (VC_LCTRL_ENABLE), its other
                                         ;   bits from the shadow
                                         ;   Sets VID_MODE b7 (disturbed)
                                         ;   Out: C set, nothing written, if no card or X > 1
                                         ;   Modifies: Flags, A, X

VdpStatus           := $A0D5             ; Read a status register through port A
                                         ;   In: X = status register (0-15, VC_STAT*)
                                         ;   Out: A = its value; C set, nothing read, if no card
                                         ;        or X > 15
                                         ;   STATSEL_A goes back to 0 without reading STAT0.
                                         ;   Reading STAT0 itself clears its flags, and STAT1 its
                                         ;   latches (see the status registers in Section 6)
                                         ;   Preserves: X, Y
                                         ;   Modifies: Flags, A


; =============================================================================
;   SECTION 5: IO HARDWARE REGISTERS
; =============================================================================

; --- Banked RAM Low (IO 1: $8000-$83FF) ---
;   256 banks × ~1 KB each.  Write bank number to latch, then access data window.
RAM_DATA_L          := $8000             ; R/W data window ($8000-$83FE)
RAM_BANK_L          := $83FF             ; Write-only bank latch ($00-$FF)

; --- Banked RAM High (IO 2: $8400-$87FF) ---
RAM_DATA_H          := $8400             ; R/W data window ($8400-$87FE)
RAM_BANK_H          := $87FF             ; Write-only bank latch ($00-$FF)

; --- RTC (DS1511Y) (IO 3: $8800-$881F) ---
RTC_SEC             := $8800             ; Seconds      (BCD, 00-59)
RTC_MIN             := $8801             ; Minutes      (BCD, 00-59)
RTC_HR              := $8802             ; Hours        (BCD, 00-23, 24h mode)
RTC_DAY             := $8803             ; Day of week  (1-7)
RTC_DATE            := $8804             ; Day of month (BCD, 01-31)
RTC_MON             := $8805             ; Month        (BCD, 01-12)
RTC_YR              := $8806             ; Year         (BCD, 00-99)
RTC_CENT            := $8807             ; Century      (BCD, 00-39)
RTC_ALARM_SEC       := $8808             ; Alarm seconds
RTC_ALARM_MIN       := $8809             ; Alarm minutes
RTC_ALARM_HR        := $880A             ; Alarm hours
RTC_ALARM_DATE      := $880B             ; Alarm day of month
RTC_WD_L            := $880C             ; Watchdog low
RTC_WD_H            := $880D             ; Watchdog high
RTC_CTRL_A          := $880E             ; Control Register A
RTC_CTRL_B          := $880F             ; Control Register B
RTC_RAM_ADDR        := $8810             ; NVRAM address register
RTC_RAM_DATA        := $8813             ; NVRAM data register
; Control Register B bits:
RTC_CTRL_B_TE       := %10000000         ; Transfer Enable — 1 = counters update the user registers once a second, 0 = inhibited
RTC_CTRL_B_BME      := %00100000         ; Burst Mode Enable — 1 = RTC_RAM_ADDR auto-increments on each access of RTC_RAM_DATA
; Month register ($8805) — the upper 3 bits are oscillator/SQW control, not month data.
; Always mask with RTC_MON_MASK when reading the month, and preserve these bits on write.
RTC_MON_MASK        := %00011111         ; Month value occupies the low 5 bits
RTC_MON_EOSC        := %10000000         ; Oscillator Start/Stop (0 = oscillator running)
RTC_MON_E32K        := %01000000         ; 32 kHz square-wave enable (1 = SQW output disabled)
RTC_MON_BB32        := %00100000         ; Battery-backup 32 kHz SQW enable (1 = enabled in backup)

; --- CompactFlash / Storage (IO 4: $8C00-$8C07) ---
;   8-bit IDE (True IDE) mode.  The BIOS issues Set Features $01 at boot
;   to enable 8-bit data transfers.
ST_DATA             := $8C00             ; R/W — Data register
ST_ERROR            := $8C01             ; Read — Error register
ST_FEATURE          := $8C01             ; Write — Feature register
ST_SECT_CNT         := $8C02             ; R/W — Sector count
ST_LBA_0            := $8C03             ; R/W — LBA bits 0-7
ST_LBA_1            := $8C04             ; R/W — LBA bits 8-15
ST_LBA_2            := $8C05             ; R/W — LBA bits 16-23
ST_LBA_3            := $8C06             ; R/W — LBA bits 24-27 + drive/mode
ST_STATUS           := $8C07             ; Read — Status register
ST_CMD              := $8C07             ; Write — Command register

; CF Status Register bits:
ST_STATUS_ERR       := %00000001         ; Error occurred
ST_STATUS_IDX       := %00000010         ; Index mark
ST_STATUS_CORR      := %00000100         ; Corrected data
ST_STATUS_DRQ       := %00001000         ; Data request — ready for data transfer
ST_STATUS_DSC       := %00010000         ; Drive seek complete
ST_STATUS_DWF       := %00100000         ; Drive write fault
ST_STATUS_RDY       := %01000000         ; Drive ready
ST_STATUS_BSY       := %10000000         ; Drive busy — all other bits invalid while set

; CF Error Register bits:
ST_ERR_AMNF         := %00000001         ; Address mark not found
ST_ERR_ABRT         := %00000100         ; Command aborted
ST_ERR_IDNF         := %00010000         ; ID not found
ST_ERR_UNC          := %01000000         ; Uncorrectable data error
ST_ERR_BBK          := %10000000         ; Bad block detected

; ATA Command bytes:
ST_CMD_READ         := $20               ; Read Sectors
ST_CMD_WRITE        := $30               ; Write Sectors
ST_CMD_SET_FEAT     := $EF               ; Set Features
ST_FEAT_8BIT        := $01               ; Feature: Enable 8-bit data I/O
ST_LBA3_MASTER      := $E0               ; LBA mode, master drive (bits 7-5=111, bit 4=0)

; Filesystem constants:
FS_DIR_LBA          = 0                  ; Directory sector at LBA 0
FS_DATA_START       = 1                  ; Data sectors start at LBA 1
FS_MAX_FILES        = 16                 ; Maximum directory entries
FS_ENTRY_SIZE       = 32                 ; Bytes per directory entry
FS_ENTRY_FLAGS      = 11                 ; Offset: flags byte within entry
FS_ENTRY_START      = 12                 ; Offset: start sector (2 bytes, little-endian)
FS_ENTRY_FSIZE      = 14                 ; Offset: file size (2 bytes, little-endian)
FS_FLAG_USED        = %00000001          ; Flags bit 0: entry in use
FS_DISK_SECTORS     = 2048               ; Sectors per disk bank (2048 × 512 bytes = 1 MB); 256 banks = 256 MB total

; XModem protocol constants (used with XModemLoad / XModemSave):
XMODEM_SOH          = $01                ; Start of Header
XMODEM_EOT          = $04                ; End of Transmission
XMODEM_ACK          = $06                ; Acknowledge
XMODEM_NAK          = $15                ; Negative Acknowledge (receiver sends to initiate transfer)
XMODEM_CAN          = $18                ; Cancel
XMODEM_SUB          = $1A                ; Padding byte (fills unused bytes in last block)
XMODEM_BLKSZ        = 128                ; Data bytes per block
XMODEM_MAXRETRY     = 10                 ; Max retries per block during transfer
XMODEM_STARTRETRY   = 60                 ; Retries waiting for initial connect (~60s)

; --- Serial Card (R65C51 ACIA) (IO 5: $9000-$9003) ---
;   Default config after InitSC: 8-N-1, 19200 baud, RX IRQ enabled
SC_DATA             := $9000             ; R/W — Data register
SC_RESET            := $9001             ; Write — Programmatic reset (write any value)
SC_STATUS           := $9001             ; Read — Status register
SC_CMD              := $9002             ; R/W — Command register
SC_CTRL             := $9003             ; R/W — Control register

; Serial Status Register bits:
SC_STATUS_IRQ       := %10000000         ; Interrupt occurred
SC_STATUS_DSR       := %01000000         ; Data Set Ready
SC_STATUS_DCD       := %00100000         ; Data Carrier Detect
SC_STATUS_TDRE      := %00010000         ; TX Data Register Empty (ready to send)
SC_STATUS_RDRF      := %00001000         ; RX Data Register Full (byte available)
SC_STATUS_OVR       := %00000100         ; Overrun error
SC_STATUS_FRME      := %00000010         ; Framing error
SC_STATUS_PARE      := %00000001         ; Parity error

; Serial Command Register bits:
SC_CMD_RXIRQ_OFF    := %00000010         ; Receiver interrupt disabled (set = off)
                                         ;   Set it before polling SC_DATA yourself,
                                         ;   as XModem does; while it is clear, Chrout
                                         ;   moves a received byte into the input buffer

; --- GPIO / Input Board (65C22 VIA) (IO 6: $9400-$940F) ---
;   Port B = matrix keyboard / joystick 1
;   Port A = PS/2 keyboard / joystick 2
;   CB1/CA1 IRQs fire when a key scancode is ready
GPIO_PORTB          := $9400             ; Port B data (active keyboard/joystick 1 data)
GPIO_PORTA          := $9401             ; Port A data (active keyboard/joystick 2 data)
GPIO_DDRB           := $9402             ; Port B data direction (0=input, 1=output per bit)
GPIO_DDRA           := $9403             ; Port A data direction
GPIO_T1CL           := $9404             ; Timer 1 counter low (read=counter, write=latch)
GPIO_T1CH           := $9405             ; Timer 1 counter high (write starts timer)
GPIO_T1LL           := $9406             ; Timer 1 latch low
GPIO_T1LH           := $9407             ; Timer 1 latch high
GPIO_T2CL           := $9408             ; Timer 2 counter low
GPIO_T2CH           := $9409             ; Timer 2 counter high
GPIO_SR             := $940A             ; Shift register
GPIO_ACR            := $940B             ; Auxiliary control register
GPIO_PCR            := $940C             ; Peripheral control register
GPIO_IFR            := $940D             ; Interrupt flag register
GPIO_IER            := $940E             ; Interrupt enable register
GPIO_ORA            := $940F             ; Port A (no handshake)

; VIA Peripheral Control Register (PCR) mode bits:
GPIO_PCR_CB2_LO     := %11000000         ; CB2 = manual output low  (enable matrix keyboard encoder)
GPIO_PCR_CB2_HI     := %11100000         ; CB2 = manual output high (disable matrix keyboard encoder)
GPIO_PCR_CB1_NEG    := %00000000         ; CB1 = active on falling edge
GPIO_PCR_CA2_LO     := %00001100         ; CA2 = manual output low  (enable PS/2 keyboard encoder)
GPIO_PCR_CA2_HI     := %00001110         ; CA2 = manual output high (disable PS/2 keyboard encoder)
GPIO_PCR_CA1_NEG    := %00000000         ; CA1 = active on falling edge

; VIA Interrupt flag/enable bits:
GPIO_INT_CB1        := %00010000         ; Bit 4: CB1 interrupt (matrix keyboard data ready)
GPIO_INT_CA1        := %00000010         ; Bit 1: CA1 interrupt (PS/2 keyboard data ready)
GPIO_IER_SET        := %10000000         ; IER bit 7: 1=set named bits, 0=clear named bits

; --- Sound Card (SID / ARMSID) (IO 7: $9800-$981C) ---
;   MOS 6581/8580 compatible.  3 voices, each with independent frequency,
;   pulse width, waveform, and ADSR envelope.  Global filter and volume.

; Voice 1:
SID_V1_FREQ_LO      := $9800             ; Frequency low byte
SID_V1_FREQ_HI      := $9801             ; Frequency high byte
SID_V1_PW_LO        := $9802             ; Pulse width low byte (bits 0-7)
SID_V1_PW_HI        := $9803             ; Pulse width high nibble (bits 0-3)
SID_V1_CTRL         := $9804             ; Control register (waveform + gate)
SID_V1_AD           := $9805             ; Attack (high nibble) / Decay (low nibble)
SID_V1_SR           := $9806             ; Sustain (high nibble) / Release (low nibble)

; Voice 2:
SID_V2_FREQ_LO      := $9807
SID_V2_FREQ_HI      := $9808
SID_V2_PW_LO        := $9809
SID_V2_PW_HI        := $980A
SID_V2_CTRL         := $980B
SID_V2_AD           := $980C
SID_V2_SR           := $980D

; Voice 3:
SID_V3_FREQ_LO      := $980E
SID_V3_FREQ_HI      := $980F
SID_V3_PW_LO        := $9810
SID_V3_PW_HI        := $9811
SID_V3_CTRL         := $9812
SID_V3_AD           := $9813
SID_V3_SR           := $9814

; Global SID registers:
SID_FC_LO           := $9815             ; Filter cutoff low (bits 0-2)
SID_FC_HI           := $9816             ; Filter cutoff high (bits 0-7)
SID_RES_FILT        := $9817             ; Resonance (high nibble) / Filter enable per voice (bits 0-2)
SID_MODE_VOL        := $9818             ; Filter mode (high nibble) / Master volume (low nibble, 0-15)
SID_PADDLE_X        := $9819             ; Paddle X (read-only)
SID_PADDLE_Y        := $981A             ; Paddle Y (read-only)
SID_OSC3            := $981B             ; Oscillator 3 output (read-only)
SID_ENV3            := $981C             ; Envelope 3 output (read-only)

; SID Voice Control Register bits (apply to SID_Vn_CTRL):
SID_CTRL_GATE       := %00000001         ; Gate: 1=start attack, 0=start release
SID_CTRL_SYNC       := %00000010         ; Hard sync with preceding voice
SID_CTRL_RING       := %00000100         ; Ring modulation with preceding voice
SID_CTRL_TEST       := %00001000         ; Test bit (resets oscillator)
SID_CTRL_TRIANGLE   := %00010000         ; Triangle waveform
SID_CTRL_SAWTOOTH   := %00100000         ; Sawtooth waveform
SID_CTRL_PULSE      := %01000000         ; Pulse waveform
SID_CTRL_NOISE      := %10000000         ; Noise waveform

; SID Filter Mode bits (high nibble of SID_MODE_VOL):
SID_FILT_LP         := %00010000         ; Low-pass filter
SID_FILT_BP         := %00100000         ; Band-pass filter
SID_FILT_HP         := %01000000         ; High-pass filter
SID_FILT_3OFF       := %10000000         ; Disconnect voice 3 from output

; --- SID Frequency Table (1 MHz clock, equal temperament) ---
;   Freq register = round(Hz × 16777216 / 1000000)
;   Covers octaves 2-7 (C2=65 Hz to B7=3951 Hz), all 12 chromatic notes.
;   Sharps and flats are provided as aliases (e.g. SID_NOTE_CS4 = SID_NOTE_Db4).
;   Use SidPlayNote: A=voice (0-2), X=freq low byte, Y=freq high byte.
;   Example: ldx #<SID_NOTE_A4 / ldy #>SID_NOTE_A4

; Octave 2 (C2=65.41 Hz .. B2=123.47 Hz)
SID_NOTE_C2         = $0449              ; ~65 Hz
SID_NOTE_CS2        = $048B              ; ~69 Hz
SID_NOTE_Db2        = SID_NOTE_CS2
SID_NOTE_D2         = $04D0              ; ~73 Hz
SID_NOTE_DS2        = $0519              ; ~78 Hz
SID_NOTE_Eb2        = SID_NOTE_DS2
SID_NOTE_E2         = $0567              ; ~82 Hz
SID_NOTE_F2         = $05B9              ; ~87 Hz
SID_NOTE_FS2        = $0610              ; ~93 Hz
SID_NOTE_Gb2        = SID_NOTE_FS2
SID_NOTE_G2         = $066C              ; ~98 Hz
SID_NOTE_GS2        = $06CE              ; ~104 Hz
SID_NOTE_Ab2        = SID_NOTE_GS2
SID_NOTE_A2         = $0735              ; ~110 Hz
SID_NOTE_AS2        = $07A3              ; ~117 Hz
SID_NOTE_Bb2        = SID_NOTE_AS2
SID_NOTE_B2         = $0817              ; ~123 Hz

; Octave 3 (C3=130.81 Hz .. B3=246.94 Hz)
SID_NOTE_C3         = $0893              ; ~131 Hz
SID_NOTE_CS3        = $0915              ; ~139 Hz
SID_NOTE_Db3        = SID_NOTE_CS3
SID_NOTE_D3         = $099F              ; ~147 Hz
SID_NOTE_DS3        = $0A32              ; ~156 Hz
SID_NOTE_Eb3        = SID_NOTE_DS3
SID_NOTE_E3         = $0ACD              ; ~165 Hz
SID_NOTE_F3         = $0B72              ; ~175 Hz
SID_NOTE_FS3        = $0C20              ; ~185 Hz
SID_NOTE_Gb3        = SID_NOTE_FS3
SID_NOTE_G3         = $0CD8              ; ~196 Hz
SID_NOTE_GS3        = $0D9C              ; ~208 Hz
SID_NOTE_Ab3        = SID_NOTE_GS3
SID_NOTE_A3         = $0E6B              ; ~220 Hz
SID_NOTE_AS3        = $0F46              ; ~233 Hz
SID_NOTE_Bb3        = SID_NOTE_AS3
SID_NOTE_B3         = $102F              ; ~247 Hz

; Octave 4 (C4=261.63 Hz .. B4=493.88 Hz) — middle octave
SID_NOTE_C4         = $1125              ; ~262 Hz (Middle C)
SID_NOTE_CS4        = $122A              ; ~277 Hz
SID_NOTE_Db4        = SID_NOTE_CS4
SID_NOTE_D4         = $133F              ; ~294 Hz
SID_NOTE_DS4        = $1464              ; ~311 Hz
SID_NOTE_Eb4        = SID_NOTE_DS4
SID_NOTE_E4         = $159A              ; ~330 Hz
SID_NOTE_F4         = $16E3              ; ~349 Hz
SID_NOTE_FS4        = $183F              ; ~370 Hz
SID_NOTE_Gb4        = SID_NOTE_FS4
SID_NOTE_G4         = $19B1              ; ~392 Hz
SID_NOTE_GS4        = $1B38              ; ~415 Hz
SID_NOTE_Ab4        = SID_NOTE_GS4
SID_NOTE_A4         = $1CD6              ; ~440 Hz (Concert A)
SID_NOTE_AS4        = $1E8D              ; ~466 Hz
SID_NOTE_Bb4        = SID_NOTE_AS4
SID_NOTE_B4         = $205E              ; ~494 Hz

; Octave 5 (C5=523.25 Hz .. B5=987.77 Hz)
SID_NOTE_C5         = $224B              ; ~523 Hz
SID_NOTE_CS5        = $2455              ; ~554 Hz
SID_NOTE_Db5        = SID_NOTE_CS5
SID_NOTE_D5         = $267E              ; ~587 Hz
SID_NOTE_DS5        = $28C8              ; ~622 Hz
SID_NOTE_Eb5        = SID_NOTE_DS5
SID_NOTE_E5         = $2B34              ; ~659 Hz
SID_NOTE_F5         = $2DC6              ; ~698 Hz
SID_NOTE_FS5        = $307F              ; ~740 Hz
SID_NOTE_Gb5        = SID_NOTE_FS5
SID_NOTE_G5         = $3361              ; ~784 Hz
SID_NOTE_GS5        = $366F              ; ~831 Hz
SID_NOTE_Ab5        = SID_NOTE_GS5
SID_NOTE_A5         = $39AC              ; ~880 Hz
SID_NOTE_AS5        = $3D1A              ; ~932 Hz
SID_NOTE_Bb5        = SID_NOTE_AS5
SID_NOTE_B5         = $40BC              ; ~988 Hz

; Octave 6 (C6=1046.50 Hz .. B6=1975.53 Hz)
SID_NOTE_C6         = $4495              ; ~1047 Hz
SID_NOTE_CS6        = $48A9              ; ~1109 Hz
SID_NOTE_Db6        = SID_NOTE_CS6
SID_NOTE_D6         = $4CFC              ; ~1175 Hz
SID_NOTE_DS6        = $518F              ; ~1245 Hz
SID_NOTE_Eb6        = SID_NOTE_DS6
SID_NOTE_E6         = $5669              ; ~1319 Hz
SID_NOTE_F6         = $5B8C              ; ~1397 Hz
SID_NOTE_FS6        = $60FE              ; ~1480 Hz
SID_NOTE_Gb6        = SID_NOTE_FS6
SID_NOTE_G6         = $66C2              ; ~1568 Hz
SID_NOTE_GS6        = $6CDF              ; ~1661 Hz
SID_NOTE_Ab6        = SID_NOTE_GS6
SID_NOTE_A6         = $7358              ; ~1760 Hz
SID_NOTE_AS6        = $7A34              ; ~1865 Hz
SID_NOTE_Bb6        = SID_NOTE_AS6
SID_NOTE_B6         = $8178              ; ~1976 Hz

; Octave 7 (C7=2093.00 Hz .. A7=3520.00 Hz; B7 overflows 16-bit register)
SID_NOTE_C7         = $892B              ; ~2093 Hz
SID_NOTE_CS7        = $9153              ; ~2217 Hz
SID_NOTE_Db7        = SID_NOTE_CS7
SID_NOTE_D7         = $99F7              ; ~2349 Hz
SID_NOTE_DS7        = $A31F              ; ~2489 Hz
SID_NOTE_Eb7        = SID_NOTE_DS7
SID_NOTE_E7         = $ACD2              ; ~2637 Hz
SID_NOTE_F7         = $B719              ; ~2794 Hz
SID_NOTE_FS7        = $C1FC              ; ~2960 Hz
SID_NOTE_Gb7        = SID_NOTE_FS7
SID_NOTE_G7         = $CD85              ; ~3136 Hz
SID_NOTE_GS7        = $D9BD              ; ~3322 Hz
SID_NOTE_Ab7        = SID_NOTE_GS7
SID_NOTE_A7         = $E6B0              ; ~3520 Hz
SID_NOTE_AS7        = $F467              ; ~3729 Hz
SID_NOTE_Bb7        = SID_NOTE_AS7
; Note: B7 (~3951 Hz = $102F0) exceeds the 16-bit SID register — not defined

; --- Video Card (6502-PICOVDP) (IO 8: $9C00-$9C03) ---
;   Two complete, independent port pairs.  Each has its own VRAM pointer,
;   read-ahead byte, command flip-flop and status selector; both reach the
;   same 64 KB of VRAM and the same registers (VBANK and VINC included).
;   Use port A in foreground code and port B in interrupt handlers, and a
;   handler never breaks a command pair half written — no sei/cli needed
;   around VDP work.  A handler that writes VBANK or VINC puts them back.
;
;   The command protocol is the TMS9918's, on either port: two writes to the
;   command port, a payload byte P and then a command byte C.
;     Register write:    P = value,       C = VC_REG_WRITE  | register (0-127)
;     VRAM write addr:   P = address low, C = VC_ADDR_WRITE | address bits 13:8
;     VRAM read addr:    P = address low, C = address bits 13:8 (prefetches)
;   Address bits 15:14 come from VBANK.  After each VC_DATA access the pointer
;   moves by VINC (reset +1), carrying into the bank bits.
;   Reading a status port returns the status register its STATSEL names, and
;   resets that pair's flip-flop — read status once to resynchronize.
VC_DATA             := $9C00             ; R/W — Port A VRAM data (pointer advances by VINC)
VC_REG              := $9C01             ; Write — Port A command (register / address)
VC_STATUS           := $9C01             ; Read — Port A status (register named by STATSEL_A)
VC_DATA2            := $9C02             ; R/W — Port B VRAM data (for interrupt handlers)
VC_REG2             := $9C03             ; Write — Port B command
VC_STATUS2          := $9C03             ; Read — Port B status (register named by STATSEL_B)

; Command byte bits (second write to VC_REG / VC_REG2):
VC_REG_WRITE        = $80                ; %1rrrrrrr — write register r with the payload
VC_ADDR_WRITE       = $40                ; %01aaaaaa — set the VRAM pointer for writing
                                         ; %00aaaaaa — set it for reading (no bit to OR in)

; Register numbers (write-only; read state back through the status registers).
;   Registers $02-$06 are aliases of L0NAME, L0ATTR, L0PAT, SPRATTR and SPRPAT,
;   so a TMS9918 program's writes to registers 2-6 reach them.  The names
;   below are the block addresses; the aliases get no names of their own.
; $00-$07 — Legacy core
VC_REG_MODE0        = $00                ; b1 M3 (see VC_MODE0_*)
VC_REG_MODE1        = $01                ; DISP, IE, M1, M2, SPRSIZE, SPRMAG (see VC_MODE1_*)
VC_REG_COLOR        = $07                ; b7:4 default foreground, b3:0 backdrop/border
; $08-$0F — Access and interrupts
VC_REG_VBANK        = $08                ; VRAM address bits 15:14 (b1:0), sampled by an address command
VC_REG_VINC         = $09                ; Pointer stride, signed -128..+127 ($00 = none; reset $01)
VC_REG_IRQEN        = $0A                ; Interrupt enables (see VC_IRQ_*)
VC_REG_IRQLINE      = $0B                ; Scanline compare, display line 0-255
VC_REG_PALBASE      = $0C                ; Palette base, b5:0 × $400 (reset $3F = VC_PALETTE)
VC_REG_VMODE        = $0D                ; Display mode (see VC_VMODE_*)
VC_REG_STATSEL_B    = $0E                ; Status register port B reads (0-15)
VC_REG_STATSEL_A    = $0F                ; Status register port A reads (0-15)
; $10-$17 — Layer 0
VC_REG_L0NAME       = $10                ; Name table base, × $400 (alias $02)
VC_REG_L0ATTR       = $11                ; Attribute table base, × $400 (alias $03)
VC_REG_L0PAT        = $12                ; Pattern table base, × $800 (alias $04)
VC_REG_L0SCRX       = $13                ; Horizontal scroll, bits 7:0 (bit 8 is VC_LCTRL_SCRX8)
VC_REG_L0SCRY       = $14                ; Vertical scroll, pixels
VC_REG_L0CTRL       = $15                ; Depth, attribute source, enable, opacity (see VC_LCTRL_*; reset $3C)
VC_REG_L0PAL        = $16                ; b3:0 palette group high bits
; $18-$1F — Layer 1 (reset: disabled, index 0 transparent)
VC_REG_L1NAME       = $18                ; Name table base, × $400
VC_REG_L1ATTR       = $19                ; Attribute table base, × $400
VC_REG_L1PAT        = $1A                ; Pattern table base, × $800
VC_REG_L1SCRX       = $1B                ; Horizontal scroll, bits 7:0 (bit 8 is VC_LCTRL_SCRX8)
VC_REG_L1SCRY       = $1C                ; Vertical scroll, pixels
VC_REG_L1CTRL       = $1D                ; As L0CTRL (reset $0C)
VC_REG_L1PAL        = $1E                ; b3:0 palette group high bits
; $20-$27 — Sprites
VC_REG_SPRATTR      = $20                ; Sprite attribute table base, × $80 (alias $05)
VC_REG_SPRPAT       = $21                ; Sprite pattern table base, × $800 (alias $06)
VC_REG_SPRCOUNT     = $22                ; Active sprite slots, 0-64 (reset 32)
VC_REG_SPRCTRL      = $23                ; Enable, collision, $D0 terminator, depth (see VC_SPRCTRL_*; reset $27)
VC_REG_SPRLIMIT     = $24                ; Sprites drawn per scanline, 1-32 (reset 16)
VC_REG_SPRPAL       = $25                ; b3:0 palette group high bits for sprites
; $28-$7F — Font, and reserved ($28-$2F are reserved for a blitter)
VC_REG_FONT         = $30                ; Load a built-in font: b7 layer, b6:0 font ID (see VC_FONT_*)
; $17, $1F, $26-$27, $28-$2F and $31-$7F are reserved: write 0.

; =============================================================================
;   SECTION 6: USEFUL CONSTANTS
; =============================================================================

; --- BIOS Version ---
BIOS_VERSION_MAJOR  = 2
BIOS_VERSION_MINOR  = 0

; --- NVRAM Save Slots (v1.6) ---
NV_SLOTS            = 16                 ; Save slots in RTC NVRAM
NV_SLOT_SIZE        = 16                 ; Bytes per slot (slot n at NVRAM n*16)
NV_SLOT_DATA        = 14                 ; Payload bytes per slot
NV_CK_SEED          = $A6                ; Checksum seed
NV_EMPTY            = 0                  ; NvStat: slot is free
NV_VALID            = 1                  ; NvStat: owner ID set, checksum agrees
NV_BAD              = 2                  ; NvStat: owner ID set, checksum does not

; --- ASCII Control Characters ---
CHAR_BEL            = $07                ; Bell (Beep via SID)
CHAR_BS             = $08                ; Backspace (cursor left + erase in video mode)
CHAR_LF             = $0A                ; Line Feed (cursor down, scroll if at bottom)
CHAR_CR             = $0D                ; Carriage Return (cursor to column 0)
CHAR_ESC            = $1B                ; Escape
CHAR_SPACE          = $20                ; Space

; --- Video Dimensions (Text mode) ---
;   The text console's layout, which the PICOVDP's Text mode keeps.
VID_COLS            = 40                 ; Screen width in columns
VID_ROWS            = 24                 ; Screen height in rows
VID_NAME_TABLE      = $0000              ; VRAM address of name table (40×24 = 960 bytes; L0NAME = $00)
VID_ATTR_TABLE      = $0400              ; VRAM address of attribute table (960 bytes, fg<<4 | bg; L0ATTR = $01)
VID_PATTERN_TABLE   = $0800              ; VRAM address of pattern/character table (2048 bytes; L0PAT = $01)

; --- 6502-PICOVDP: MODE0 / MODE1 bits ---
;   M1, M2 and M3 select the display mode only while VMODE is VC_VMODE_LEGACY.
VC_MODE0_M3         = %00000010          ; M3 legacy mode select
VC_MODE1_DISP       = %01000000          ; Display enable (clear: backdrop only)
VC_MODE1_IE         = %00100000          ; Vertical blank IRQ enable (alias of VC_IRQ_VBLANK in IRQEN)
VC_MODE1_M1         = %00010000          ; M1 legacy mode select (Text)
VC_MODE1_M2         = %00001000          ; M2 legacy mode select
VC_MODE1_SPRSIZE    = %00000010          ; Sprite size: 0 = 8×8, 1 = 16×16
VC_MODE1_SPRMAG     = %00000001          ; Magnify sprites ×2

; --- 6502-PICOVDP: VMODE values ---
VC_VMODE_LEGACY     = $0                 ; M1/M2/M3 select the mode (reset); $5-$F behave as $0
VC_VMODE_TEXT       = $1                 ; 40×24 cells of 6×8, 240×192
VC_VMODE_COMPACT    = $2                 ; 32×24 cells of 8×8, 256×192
VC_VMODE_GRAPHICS   = $3                 ; 32×30 cells of 8×8, 256×240
VC_VMODE_FULL       = $4                 ; 40×30 cells of 8×8, 320×240, no border

; --- 6502-PICOVDP: L0CTRL / L1CTRL bits ---
;   Pick one depth and one attribute source, then OR in the flags.
VC_LCTRL_1BPP       = $00                ; b1:0 bit depth — 8 bytes per tile
VC_LCTRL_2BPP       = $01                ;   16 bytes per tile
VC_LCTRL_4BPP       = $02                ;   32 bytes per tile
VC_LCTRL_8BPP       = $03                ;   64 bytes per tile
VC_LCTRL_ATTR_CELL  = $00                ; b3:2 attribute source — one byte per cell at LxATTR + cell
VC_LCTRL_ATTR_GROUP = $04                ;   one byte per 8 patterns at LxATTR + (pattern >> 3)
VC_LCTRL_ATTR_ROW   = $08                ;   one byte per pattern row at LxATTR + pattern × 8 + row
VC_LCTRL_ATTR_NONE  = $0C                ;   no attribute fetch
VC_LCTRL_ENABLE     = $10                ; b4 layer enable
VC_LCTRL_OPAQUE     = $20                ; b5 index 0 opaque
VC_LCTRL_SCRX8      = $40                ; b6 horizontal scroll bit 8 (Full mode's 320 pixels)

; --- 6502-PICOVDP: attribute byte (layers at 2/4/8bpp, and sprites) ---
;   At 1bpp a layer's colour byte is foreground (b7:4) and background (b3:0).
VC_ATTR_SUBPAL      = $0F                ; b3:0 sub-palette (ignored by an 8bpp layer)
VC_ATTR_FLIPH       = $10                ; b4 flip horizontally
VC_ATTR_FLIPV       = $20                ; b5 flip vertically
VC_ATTR_PRIORITY    = $40                ; b6 priority (layer tile in front of sprites; sprite in front of layer 1)
VC_ATTR_BIT8        = $80                ; b7 layer: pattern index bit 8 (2/4bpp); sprite: X bit 8

; --- 6502-PICOVDP: SPRCTRL bits ---
VC_SPRCTRL_ENABLE   = $01                ; b0 sprites enable
VC_SPRCTRL_COLLIDE  = $02                ; b1 collision detection enable
VC_SPRCTRL_TERM     = $04                ; b2 a Y of VC_SPRITE_END ends the sprite list
VC_SPRCTRL_DETAIL   = $08                ; b3 detailed collision map (STAT8-STAT15)
VC_SPRCTRL_1BPP     = $00                ; b5:4 sprite bit depth
VC_SPRCTRL_2BPP     = $10
VC_SPRCTRL_4BPP     = $20
VC_SPRCTRL_8BPP     = $30

; --- 6502-PICOVDP: interrupt sources (IRQEN bits, latched in STAT1) ---
VC_IRQ_VBLANK       = $01                ; b0 end of the active picture, once a frame
VC_IRQ_SCANLINE     = $02                ; b1 display line matches IRQLINE
VC_IRQ_OVERFLOW     = $04                ; b2 a sprite was dropped, at most once a frame
VC_IRQ_COLLISION    = $08                ; b3 two sprites collided, at most once a frame

; --- 6502-PICOVDP: status registers (values for STATSEL_A / STATSEL_B) ---
;   Reading STAT0 clears its flags, STAT7, the collision map and the vblank,
;   overflow and collision latches in STAT1.  Reading STAT1 clears only the
;   STAT1 latches — so a handler on port B reads STAT1 and foreground code
;   polling STAT0 on port A still sees F.  Read STAT7 and the collision map
;   before STAT0.
VC_STAT0            = 0                  ; F, OVF, COL, first sprite dropped (TMS9918 status)
VC_STAT1            = 1                  ; Latched interrupt sources still enabled in IRQEN
VC_STAT2            = 2                  ; Current display line, low 8 bits
VC_STAT3            = 3                  ; Blanking (see VC_STAT3_*)
VC_STAT4            = 4                  ; Identification byte, VC_ID
VC_STAT5            = 5                  ; Firmware version, BCD (high nibble major, low nibble minor)
VC_STAT6            = 6                  ; Capabilities (see VC_CAP_*)
VC_STAT7            = 7                  ; Index (0-63) of the first sprite dropped on the latest overflowing line
VC_STAT_COLLIDE     = 8                  ; STAT8-STAT15: collision map, sprite s is bit s mod 8 of STAT(8 + s/8)

VC_STAT0_F          = $80                ; A picture has ended (set whatever IRQEN holds)
VC_STAT0_OVF        = $40                ; A sprite has been dropped
VC_STAT0_COL        = $20                ; Two sprites have collided
VC_STAT0_SPRITE     = $1F                ; Low five bits of the first sprite dropped

VC_STAT3_VBLANK     = $01                ; Vertical blanking (borders included)
VC_STAT3_HBLANK     = $02                ; Horizontal blanking (advisory)

VC_ID               = $AC                ; STAT4 on a 6502-PICOVDP

VC_CAP_LAYER2       = $01                ; STAT6 b0 two layers
VC_CAP_8BPP         = $02                ; STAT6 b1 8bpp layer
VC_CAP_FLIP         = $04                ; STAT6 b2 sprite flip
VC_CAP_SCROLL       = $08                ; STAT6 b3 hardware scroll
VC_CAP_SCANLINE     = $10                ; STAT6 b4 scanline IRQ
VC_CAP_VRAM64K      = $20                ; STAT6 b5 64 KB VRAM
                                         ; STAT6 b6 reserved (a blitter)
VC_CAP_FONT         = $80                ; STAT6 b7 built-in font: VC_REG_FONT and font $00, loaded at reset

; --- 6502-PICOVDP: FONT values ---
;   Reset writes font VC_FONT_CP437 to $0800-$0FFF (VID_PATTERN_TABLE), so a
;   program that selects Text and sets L0PAT = $01 finds it already there.
;   InitVideo loads it again, and VdpLoadFont loads it for layer 0 and waits.
;   A write to VC_REG_FONT loads a font into a layer's pattern table:
;   OR a layer into a font ID, e.g. VC_FONT_LAYER1 | VC_FONT_CP437.
;     * Every write is a command, even of the value already held.
;     * The destination is LxPAT × $800, sampled at the write; a later write
;       to L0PAT/L1PAT does not move a pending load.
;     * A reserved font ID ($01-$7F) is stored and does nothing else.
;     * One load is pending per layer; a second write for that layer replaces it.
;     * The load completes at the line start where vertical blank fires, before
;       VC_STAT0_F sets.  Until then the table keeps its old contents, and a
;       write there is overwritten.  So: read STAT0 to clear F, write
;       VC_REG_FONT, and wait for F before touching that range.
;     * A reset cancels every pending load.
;   Check VC_CAP_FONT in STAT6 first.
VC_FONT_LAYER0      = $00                ; b7 clear: layer 0's pattern table (L0PAT × $800)
VC_FONT_LAYER1      = $80                ; b7 set: layer 1's pattern table (L1PAT × $800)
VC_FONT_CP437       = $00                ; Font $00: CP437, 6×8, 2,048 bytes (glyph n at n × 8)

; --- 6502-PICOVDP: VRAM ---
VC_PALETTE          = $FC00              ; Palette at reset PALBASE: 256 entries of %0000RRRR, %GGGGBBBB
VC_SPRITE_END       = $D0                ; Sprite Y that ends the list while VC_SPRCTRL_TERM is set

; --- Colour Constants (palette row 0) ---
;   The PICOVDP's default palette puts the sixteen TMS9918 colours in row 0
;   at their TMS9918 indices, so these name the same colours they always did.
;   At 1bpp they are the foreground and background nibbles, and for
;   VideoSetColor (the pen; its background nibble is also the border).
;   Index 0 is transparent unless the layer is opaque.
;   Pass as: (foreground << 4) | background
;   Example: white on black = $F0, green on black = $20
TMS_TRANSPARENT     = $0
TMS_BLACK           = $1
TMS_MED_GREEN       = $2
TMS_LT_GREEN        = $3
TMS_DK_BLUE         = $4
TMS_LT_BLUE         = $5
TMS_DK_RED          = $6
TMS_CYAN            = $7
TMS_MED_RED         = $8
TMS_LT_RED          = $9
TMS_DK_YELLOW       = $A
TMS_LT_YELLOW       = $B
TMS_DK_GREEN        = $C
TMS_MAGENTA         = $D
TMS_GRAY            = $E
TMS_WHITE           = $F
