.setcpu "65C02"

.include "6502.inc"

.segment "CODE"

BasicStartup: .byte $0A, $08, $0A, $00, $9E, $32, $30, $36, $30, $00, $00, $00    ; BASIC = 10 SYS 2060 

Start:
  ; Your code goes here!
  rts 