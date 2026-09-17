.setcpu "65C02"

; make VDP=1 builds for an ACE with a 6502-PICOVDP on BIOS 2.x;
; the default builds for the TMS9918A on BIOS 1.x.
.ifdef VDP
.include "6502-VDP.inc"
.else
.include "6502.inc"
.endif

.segment "CODE"

; =============================================================================
;   BASIC Startup Stub
; =============================================================================
;   A tokenized BASIC line: 10 SYS 2060
;   When this program is loaded into $0800 and RUN in BASIC, the SYS command
;   jumps to the machine code entry point at $080C (decimal 2060).
;   This stub must remain at the very start of the program.

BasicStartup: .byte $0A, $08, $0A, $00, $A5, $32, $30, $36, $30, $00, $00, $00

; =============================================================================
;   Start — Program entry point ($080C)
; =============================================================================
;   The system is already fully initialized when your program runs:
;     - Hardware probed and initialized (HW_PRESENT is set)
;     - Interrupts enabled, keyboard active
;     - IO_MODE set (video or serial console)
;     - All Kernal jump table routines available
;
;   Return to BASIC with RTS.
; =============================================================================

Start:
.ifdef VDP
  ; --- VDP build only: refuse to run on BIOS 1.x ---
  ; A program built with 6502-VDP.inc may call 2.x Kernal entries that are
  ; bare RTS slots on 1.x, so it says so and returns to BASIC instead.
  jsr KernalVersion             ; A = major, X = minor
  cmp #2
  bcs @Bios2
  lda #<NeedsBios2Msg
  ldy #>NeedsBios2Msg
  jsr PrintStr
  rts                           ; Return to BASIC
@Bios2:
.endif

  ; === Your program starts here ===

  ; Example: clear screen and print a message
  jsr VideoClear                ; Clear video screen

  lda #<HelloMsg
  ldy #>HelloMsg
  jsr PrintStr                  ; Print the message

@WaitKey:
  jsr Chrin                     ; Poll for a keypress (non-blocking)
  bcc @WaitKey                  ; Loop until character available (C=1)

  jsr VideoClear                ; Clear screen before returning
  rts                           ; Return to BASIC

; =============================================================================
;   Data
; =============================================================================

HelloMsg:
  .byte "Hello from Program!", CHAR_CR, CHAR_LF
  .byte "Press any key to return to BASIC.", CHAR_CR, CHAR_LF, $00

.ifdef VDP
NeedsBios2Msg:
  .byte "NEEDS BIOS 2 AND A 6502-PICOVDP", CHAR_CR, CHAR_LF, $00
.endif