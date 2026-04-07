.setcpu "65C02"

.include "6502.inc"

.segment "CODE"

; =============================================================================
;   BASIC Startup Stub
; =============================================================================
;   A tokenized BASIC line: 10 SYS 2060
;   When this program is loaded into $0800 and RUN in BASIC, the SYS command
;   jumps to the machine code entry point at $080C (decimal 2060).
;   This stub must remain at the very start of the program.

BasicStartup: .byte $0A, $08, $0A, $00, $9E, $32, $30, $36, $30, $00, $00, $00

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
  ; === Your program starts here ===

  ; Example: clear screen and print a message
  jsr VideoClear                ; Clear video screen

  lda #<HelloMsg
  sta STR_PTR
  lda #>HelloMsg
  sta STR_PTR + 1
  jsr PrintStr                  ; Print the message

  jsr Chrin                     ; Wait for a keypress

  jsr VideoClear                ; Clear screen before returning
  rts                           ; Return to BASIC

; =============================================================================
;   PrintStr — Print a null-terminated string via Chrout
;   In: STR_PTR ($02-$03) = pointer to string
; =============================================================================
PrintStr:
  ldy #$00
@PrintLoop:
  lda (STR_PTR),y
  beq @PrintDone                ; Null terminator — done
  jsr Chrout
  iny
  bne @PrintLoop                ; Max 256 chars per call
@PrintDone:
  rts

; =============================================================================
;   Data
; =============================================================================

HelloMsg:
  .byte "Hello from Program!", CHAR_CR, CHAR_LF
  .byte "Press any key to return to BASIC.", CHAR_CR, CHAR_LF, $00