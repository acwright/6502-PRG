; =============================================================================
;   6502.inc — External include file for the 6502 system
; =============================================================================
;
;   CPU:       WDC 65C02
;   BIOS:      v1.0
;   Assembler: ca65 (cc65 toolchain) — uses := for absolute symbols
;
;   Include this file in external projects to access Kernal routines,
;   IO hardware registers, system variables, and useful constants.
;
;   MEMORY MAP
;   ----------
;   $0000-$0003   Zero Page: System pointers (see below)
;   $0004-$0023   Zero Page: Reserved by BASIC interpreter
;   $0024-$0035   Zero Page: System scratch (CF, serial, delay, monitor)
;   $0036-$00FF   Zero Page: Free for user programs
;   $0100-$01FF   CPU Stack
;   $0200-$02FF   Input Buffer (ring buffer, managed by Kernal)
;   $0300-$03FF   Kernal Variables (vectors, cursor, HW flags, etc.)
;   $0400-$07FF   User Variables / BASIC workspace
;   $0800-$7FFF   Program RAM (30 KB — load address for user code)
;   $8000-$83FF   IO 1: Banked RAM Low  (256 × ~1 KB banks)
;   $8400-$87FF   IO 2: Banked RAM High (256 × ~1 KB banks)
;   $8800-$881F   IO 3: RTC (DS1511Y)
;   $8C00-$8C07   IO 4: Storage (CompactFlash, 8-bit IDE)
;   $9000-$9003   IO 5: Serial (R65C51 ACIA)
;   $9400-$940F   IO 6: GPIO (65C22 VIA — keyboard & joysticks)
;   $9800-$981C   IO 7: Sound (SID / ARMSID)
;   $9C00-$9C01   IO 8: Video (TMS9918 / pico9918)
;   $A000-$A0FF   Kernal Jump Table (stable entry points)
;   $A100-$B7FF   Kernal Implementation
;   $B800-$BFFF   Character Set ROM (2 KB, 256 × 8-byte patterns)
;   $C000-$DFFF   Machine Code Monitor (8 KB)
;   $E000-$FEFF   BASIC Interpreter
;   $FF00-$FFF9   Wozmon (mini monitor)
;   $FFFA-$FFFF   CPU Vectors (NMI, RESET, IRQ)
;
;   BOOT SEQUENCE
;   -------------
;   On RESET the BIOS probes all eight IO slots, initializes detected
;   hardware, and auto-selects video or serial console.  If neither is
;   found, IO_MODE is left unchanged (no halt — cartridges with their
;   own display hardware can still boot).  A startup beep plays (if SID
;   is present) for audible feedback, then BOOT_VECTOR ($035B) is
;   checked — if non-zero, execution jumps there (cartridge takeover).
;   Otherwise a console check halts if no video/serial is found, then
;   the splash screen and boot menu are displayed.  ENTER launches
;   BASIC; ESC enters the Monitor.  A 5-second timeout auto-boots BASIC.
;
;   CARTRIDGE SUPPORT
;   -----------------
;   Cartridges overlay $C000-$FFFF (Monitor, BASIC, Wozmon, vectors).
;   The Kernal ($A000-$B7FF) and character set ($B800-$BFFF) remain
;   accessible.  Use KernalInit ($A072) to initialize all hardware and
;   return via RTS — interrupts are left disabled so the cart can set
;   up its own handlers first.  See usage patterns in the BIOS README.
;
;   IO_MODE
;   -------
;   Bit 0 of IO_MODE selects the console output device:
;     0 = Video (TMS9918)    1 = Serial (R65C51)
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
;     and #HW_VID          ; Is video card present?
;     beq @NoVideo         ; Skip if not
;
; =============================================================================


; =============================================================================
;   SECTION 1: ZERO PAGE POINTERS
; =============================================================================
; These zero page locations are used by the Kernal.  External programs may
; use STR_PTR and CF_BUF_PTR/CF_LBA as parameters to Kernal calls.
; Locations $36-$FF are free for user programs.

STR_PTR             := $02               ; 2 bytes — String pointer (used by FsLoadFile, FsSaveFile, FsDeleteFile, etc.)
CF_BUF_PTR          := $24               ; 2 bytes — CF sector data buffer pointer
CF_LBA              := $26               ; 4 bytes — CF 28-bit LBA address (little-endian)

; Safe zero page range for user programs:
; $36-$FF (202 bytes free)


; =============================================================================
;   SECTION 2: SYSTEM VARIABLES ($0300-$03FF)
; =============================================================================

IRQ_PTR             := $0300             ; 2 bytes — IRQ handler vector (writable; default: internal Irq handler)
BRK_PTR             := $0302             ; 2 bytes — BRK handler vector (writable; default: enters Monitor)
NMI_PTR             := $0304             ; 2 bytes — NMI handler vector (writable; default: RTI)

IO_MODE             := $0306             ; 1 byte  — Console output mode (bit 0: 0=video, 1=serial)
VID_CURSOR_X        := $0307             ; 1 byte  — Video cursor column (0-39)
VID_CURSOR_Y        := $0308             ; 1 byte  — Video cursor row (0-23)

HW_PRESENT          := $030D             ; 1 byte  — Hardware present bitmask (set during boot probe)

; HW_PRESENT bit definitions (bit order matches IO slot numbers):
HW_RAM_L            = %00000001          ; Bit 0: RAM card low  (IO 1, $8000-$83FF)
HW_RAM_H            = %00000010          ; Bit 1: RAM card high (IO 2, $8400-$87FF)
HW_RTC              = %00000100          ; Bit 2: RTC card      (IO 3, DS1511Y)
HW_CF               = %00001000          ; Bit 3: Storage card   (IO 4, CompactFlash)
HW_SC               = %00010000          ; Bit 4: Serial card    (IO 5, R65C51)
HW_GPIO             = %00100000          ; Bit 5: GPIO card      (IO 6, 65C22 VIA — keyboard/joysticks)
HW_SID              = %01000000          ; Bit 6: Sound card     (IO 7, SID/ARMSID)
HW_VID              = %10000000          ; Bit 7: Video card     (IO 8, TMS9918/pico9918)

; BRK register save area (populated by the BRK handler before entering Monitor):
BRK_P               := $0310             ; Saved processor status (P)
BRK_PCL             := $0311             ; Saved PC low byte (points to BRK instruction + 2)
BRK_PCH             := $0312             ; Saved PC high byte
BRK_A               := $0313             ; Saved accumulator
BRK_X               := $0314             ; Saved X register
BRK_Y               := $0315             ; Saved Y register
BRK_SP              := $0316             ; Saved stack pointer

; RTC scratch (used by RtcWriteDate to pass century):
RTC_BUF_CENT        := $030B             ; Century value for RtcReadDate / RtcWriteDate

; Cartridge / boot redirect vector:
BOOT_VECTOR         := $035B             ; 2 bytes — Boot redirect address (0=normal boot)
                                         ;   Set after KernalInit to redirect Reset flow
                                         ;   KernalInit zeroes this; write after init returns


; =============================================================================
;   SECTION 3: KEY MEMORY LOCATIONS
; =============================================================================

INPUT_BUFFER        := $0200             ; $0200-$02FF — 256-byte ring buffer (keyboard/serial input)
PROGRAM_START       := $0800             ; User programs and BASIC programs load here
;                                        ; Program space extends to $7FFF (30 KB)


; =============================================================================
;   SECTION 4: KERNAL JUMP TABLE ($A000-$A0FF)
; =============================================================================
; All entry points are stable 3-byte JMP instructions.  Call via JSR.
; Unimplemented slots return immediately (RTS).

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

; --- Video (TMS9918 / pico9918) ---
InitVideo           := $A015             ; Initialize TMS9918 (text mode, 40×24)
                                         ;   Modifies: A, X

VideoClear          := $A018             ; Clear screen (fill with spaces), reset cursor to (0,0)
                                         ;   Modifies: A, X, Y

VideoPutChar        := $A01B             ; Write character at cursor position (no cursor advance)
                                         ;   In: A = character

VideoSetCursor      := $A01E             ; Set cursor position
                                         ;   In: X = column (0-39), Y = row (0-23)
                                         ;   Modifies: A

VideoGetCursor      := $A021             ; Get cursor position
                                         ;   Out: X = column (0-39), Y = row (0-23)

VideoScroll         := $A024             ; Scroll screen up one line, clear bottom row
                                         ;   Modifies: A, X, Y

VideoSetColor       := $A027             ; Set TMS9918 text color
                                         ;   In: A = color byte (high nibble = foreground, low nibble = background)
                                         ;   See TMS9918 Color Constants below

VideoChroutRaw      := $A02A             ; Output char to video with no control-code handling
                                         ;   In: A = character code (0-255)
                                         ;   Preserves: A, X, Y
                                         ;   Always renders glyph, never interprets CR/LF/BS/BEL

; --- Sound (SID / ARMSID) ---
InitSID             := $A02D             ; Initialize SID (clear all registers, volume=15)
                                         ;   Modifies: A, X

Beep                := $A030             ; Play short beep tone (~475 Hz)
                                         ;   Skips silently if no SID detected
                                         ;   Modifies: A, X, Y

SidPlayNote         := $A033             ; Play note on a SID voice
                                         ;   In: A = voice (0-2), X = freq low, Y = freq high
                                         ;   Uses triangle waveform, ADSR: A=0 D=9 S=A R=2
                                         ;   Modifies: A

SidSilence          := $A036             ; Silence all 3 SID voices (gate off, zero freq)
                                         ;   Modifies: A

SidSetVolume        := $A039             ; Set SID master volume
                                         ;   In: A = volume (0-15)
                                         ;   Modifies: A

; --- Filesystem (simple 16-file directory on CF) ---
;   Directory lives at LBA 0 (512 bytes, 16 × 32-byte entries).
;   Data sectors start at LBA 1.
;   Filenames are 8.3 format, space-padded, case-insensitive.
;   Pass null-terminated "NAME.EXT" string via STR_PTR ($02-$03).

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
ReadJoystick1       := $A048             ; Read joystick 1 (VIA Port B)
                                         ;   Out: A = bitmask (active-low bits, active bits depend on wiring)
                                         ;   Temporarily disables matrix keyboard encoder

ReadJoystick2       := $A04B             ; Read joystick 2 (VIA Port A)
                                         ;   Out: A = bitmask (active-low bits, active bits depend on wiring)
                                         ;   Temporarily disables PS/2 keyboard encoder

; --- Serial (R65C51) ---
InitSC              := $A04E             ; Initialize serial card (8-N-1, 19200 baud)
                                         ;   Modifies: A

SerialChrout        := $A051             ; Direct serial output (bypasses IO_MODE)
                                         ;   In: A = character
                                         ;   Blocks until TX register is empty

; --- Serial Transfer ---
AsciiLoad           := $A054             ; Receive raw binary via serial into PROGRAM_START
                                         ;   Protocol: 2-byte size (lo/hi), then raw data bytes
                                         ;   Temporarily switches IO_MODE to serial
                                         ;   Out: C=0 success
                                         ;   Modifies: A, X, Y

AsciiSave           := $A057             ; Send program memory as raw binary via serial
                                         ;   Protocol: 2-byte size (lo/hi), then raw data bytes
                                         ;   Sends from PROGRAM_START to BAS_PRGEND
                                         ;   Temporarily switches IO_MODE to serial
                                         ;   Out: C=0 success
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
                                         ;   Modifies: A, X, Y

; --- Timing ---
SysDelay            := $A075             ; Delay for specified centiseconds (10 ms units)
                                         ;   In: A = low byte, X = high byte (16-bit count)
                                         ;   Uses VIA T1 if available, else software loop
                                         ;   Modifies: A

; --- System ---
KernalInit          := $A078             ; Initialize all hardware (vectors, probe & init all I/O)
                                         ;   Caller MUST reset the stack pointer before JSR:
                                         ;     ldx #$ff / txs / jsr KernalInit
                                         ;   Zeroes BOOT_VECTOR, sets HW_PRESENT, IO_MODE, IRQ/BRK/NMI ptrs
                                         ;   Does NOT reset the stack pointer
                                         ;   Does NOT enable interrupts (caller must CLI)
                                         ;   Does NOT play beep, display splash, or enter boot menu
                                         ;   Does NOT halt if no console is detected
                                         ;   Modifies: All registers, flags

KernalVersion       := $A07B             ; Get BIOS version
                                         ;   Out: A = major version, X = minor version


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
RTC_CTRL_B_TE       := %10000000         ; Transfer Enable — set to inhibit clock updates during write

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

; --- Video Card (TMS9918 / pico9918) (IO 8: $9C00-$9C01) ---
;   Text mode: 40 columns × 24 rows.  Name table at VRAM $0000 (960 bytes).
;   Pattern table at VRAM $0800 (2 KB, 256 × 8-byte character patterns).
;   To write to a VRAM address: write low byte to VC_REG, then (high | $40) to VC_REG.
;   To read from a VRAM address: write low byte to VC_REG, then high byte to VC_REG.
;   Register writes: write data byte to VC_REG, then (reg# | $80) to VC_REG.
VC_DATA             := $9C00             ; R/W — VRAM data (auto-increments address)
VC_REG              := $9C01             ; Write — Address/register port
VC_STATUS           := $9C01             ; Read — Status register

; =============================================================================
;   SECTION 6: USEFUL CONSTANTS
; =============================================================================

; --- BIOS Version ---
BIOS_VERSION_MAJOR  = 1
BIOS_VERSION_MINOR  = 0

; --- ASCII Control Characters ---
CHAR_BEL            = $07                ; Bell (Beep via SID)
CHAR_BS             = $08                ; Backspace (cursor left + erase in video mode)
CHAR_LF             = $0A                ; Line Feed (cursor down, scroll if at bottom)
CHAR_CR             = $0D                ; Carriage Return (cursor to column 0)
CHAR_ESC            = $1B                ; Escape
CHAR_SPACE          = $20                ; Space

; --- Video Dimensions ---
VID_COLS            = 40                 ; Screen width in columns
VID_ROWS            = 24                 ; Screen height in rows
VID_NAME_TABLE      = $0000              ; VRAM address of name table (40×24 = 960 bytes)
VID_PATTERN_TABLE   = $0800              ; VRAM address of pattern/character table (2048 bytes)

; --- TMS9918 Color Constants (for VideoSetColor) ---
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

; --- Monitor Entry Points ---
;   The machine code monitor lives at $C000 and is entered normally via BRK.
;   These are useful if you want to jump there directly.
MONITOR_ENTRY       := $C000             ; Cold entry (from boot menu or user code)
MONITOR_BRK_ENTRY   := $C003             ; BRK entry (displays saved registers)
