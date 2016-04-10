; ------------------------------------------------------------------------
;
; Title:
;
;   PD15 -- PIC 10 MHz to three frequencies divider (1-1k-10k Hz), with sync
;
; Function:
;
;   This PIC program implements a digital frequency divider: cpu hardware
;   and isochronous software divide the input clock by 7 powers of ten.
;   This allows generation of decade outputs from 10 kHz to 1 Hz (1PPS).
;
;   - In this version a 10 MHz input clock is divided to create three
;     simultaneous square wave outputs: 1 Hz, 1 kHz, 10 kHz.
;
;   - Two inputs support optional manual 1PPS synchronization. Pull Arm
;     pin low for a second to stop divider. All outputs will synchronize
;     to next rising edge of Sync pin (within one instruction cycle).
;
; Diagram:
;                                ---__---
;                5V (Vdd)  +++++|1      8|=====  Ground (Vss)
;         10 MHz clock in  ---->|2  pD  7|---->  10 kHz out
;                1 Hz out  <----|3  15  6|---->  1 kHz out
;                     Arm  o--->|4      5|<+---  Sync
;                                --------
; Notes:
;
;   o External pull-up required on Arm input (pin4/GP3).
;   + Sync input (pin5/GP2) has internal WPU.
;   Output frequency accuracy is the same as clock input accuracy.
;   Output drive current is 25 mA maximum per pin.
;   Coded for Microchip 12F675 but any '609 '615 '629 '635 '675 '683 works.
;
; Version:
;
;   29-Jan-2013  Tom Van Baak (tvb)  www.LeapSecond.com/pic
;
; ------------------------------------------------------------------------

; Microchip MPLAB IDE assembler code (mpasm).

        list        p=pic12f675
        include     p12f675.inc
        __config    _EC_OSC & _MCLRE_OFF & _WDT_OFF

; Register definitions.

        cblock  0x20            ; define register base
            gpcopy              ; shadow of output pins
            dig4, dig3, dig2, dig1, dig0
        endc

; Define entry points.

        org     0               ; power-on entry
        goto    init            ;
        org     4               ; interrupt entry
        goto    sync            ;

; One-time PIC 12F675 initialization.

init:   bcf     STATUS,RP0      ; bank 0
        movlw   0x07            ; turn comparator off
        movwf   CMCON           ;
        clrf    GPIO            ; set output latches low

        bsf     STATUS,RP0      ; bank 1
        errorlevel -302
        clrf    ANSEL           ; all digital (no analog) pins
        movlw   ~(1<<GP4 | 1<<GP1 | 1<<GP0)
        movwf   TRISIO          ; set pin directions (0=output)
        movlw   1<<GP2
        movwf   WPU             ; enable weak pullup (1=enable)
        movlw   1<<INTEDG       ; WPU, GP2/INT rising edge trigger
        movwf   OPTION_REG      ;
        errorlevel +302
        bcf     STATUS,RP0      ; bank 0
        call    clear           ; initialize counter and pins

; To create multiple frequency outputs the PIC increments a virtual
; '7490-style decade counter chain in a continuous isochronous loop.
; Clocking the counter at twice the output rate allows each LSB to
; generate a square wave at the desired decade frequency.
;
; A 50 us (20 kHz) toggle loop can generate a 10 kHz square wave.
; With a 10 MHz clock (400 ns Tcy) 125 instructions is 50 us.

loop:   movf    gpcopy,W        ; gpcopy -> W
        movwf   GPIO            ; W -> GPIO
        call    Delay5          ; (sync alignment)
sync:   call    armed           ; check for Arm request

        ; Update counter and map each output pin to decade LSB.

        call    count           ; increment counter
        clrf    gpcopy          ;
        btfss   dig0,0          ; 10 kHz decade LSB
          bsf   gpcopy,GP0      ;
        btfss   dig1,0          ; 1 kHz decade LSB
          bsf   gpcopy,GP1      ;
        btfss   dig4,0          ; 1 Hz decade LSB
          bsf   gpcopy,GP4      ;

        ; Pad loop for exactly 125 instructions (use MPLAB SIM).

        movlw   d'67'           ;
        call    DelayW1         ; delay (15 <= W <= 255)
        goto    loop            ;

; Initialize counter (zero) and output pins (high).

clear:  clrf    dig0
        clrf    dig1
        clrf    dig2
        clrf    dig3
        clrf    dig4
        movlw   1<<GP4 | 1<<GP1 | 1<<GP0
        movwf   gpcopy
        return

; Increment 5-digit decimal counter (isochronous code).

count:  incf    dig0,F          ; always increment LSDigit
        movlw   d'10'           ;
        subwf   dig0,W          ; check overflow
        skpnz                   ;
          clrf  dig0            ; reset to zero

        skpnz                   ;
          incf  dig1,F          ; apply previous carry
        movlw   d'10'           ;
        subwf   dig1,W          ; check overflow
        skpnz                   ;
          clrf  dig1            ; reset to zero

        skpnz                   ;
          incf  dig2,F          ; apply previous carry
        movlw   d'10'           ;
        subwf   dig2,W          ; check overflow
        skpnz                   ;
          clrf  dig2            ; reset to zero

        skpnz                   ;
          incf  dig3,F          ; apply previous carry
        movlw   d'10'           ;
        subwf   dig3,W          ; check overflow
        skpnz                   ;
          clrf  dig3            ; reset to zero

        skpnz                   ;
          incf  dig4,F          ; apply previous carry
        movlw   d'10'           ;
        subwf   dig4,W          ; check overflow
        skpnz                   ;
          clrf  dig4            ; reset to zero

        return

; Implement two-pin 1PPS Arm-Sync synchronization protocol.
; - Accept Arm (low) request when output(s) are high.
; - Use GP2/INT interrupt to keep accuracy within 1 Tcy.
; - Divider resets and resumes on rising edge of Sync pin.
; - Re-enter main loop late to compensate for interrupt/code latency.

armed:  movf    GPIO,W          ; GPIO -> W
        andlw   (1<<GP4 | 1<<GP3 | 1<<GP1 | 1<<GP0)
        xorlw   (1<<GP4 |          1<<GP1 | 1<<GP0)
        skpz                    ; Arm low, output(s) high?
          return                ;   no, continue running

        call    clear           ; initialize counter and pins
        movlw   1<<GIE|1<<INTE  ; enable GP2 edge-trigger interrupt
        movwf   INTCON          ;   (and clear interrupt flags)
        goto    $               ; no deposit, no return, no retfie

        include delayw.asm      ; precise delay functions
        end
