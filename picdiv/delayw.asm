;
; Below is a collection of variable and fixed PIC delay loops.
;
;   25-Jul-2008  Tom Van Baak (tvb)  www.LeapSecond.com/pic
;

        cblock
            tmp1, tmp100, tmp10k
        endc

; ----------------------------------------------------------------------------
;
; DelayW1 -- Delay { W x 1 } cycles, where W = 15 to 255 (270).
;
;   - Delay includes call/return time (but not load W time).
;   - Registers destroyed: tmp1, W, STATUS.
;   - WARNING: minimum valid W for this function is 15.
;
DelayW1:
        addlw   -d'15'          ; subtract function overhead
        movwf   tmp1            ;
        btfsc   tmp1,0          ; if bit 0 on, delay extra 1 cycle
          goto  $+1             ;
        btfsc   tmp1,1          ; if bit 1 on, delay extra 2 cycles
          goto  $+1             ;
        btfsc   tmp1,1          ;
          goto  $+1             ;

_d4     addlw   -4              ; delay 4 cycles per loop
        skpnc                   ;
          goto  _d4             ;
        return                  ;

; ----------------------------------------------------------------------------
;
; DelayW100 -- Delay { W x 100 } cycles, where W = 2 to 255.
;
;   - Delay includes call/return time (but not load W time).
;   - Registers destroyed: tmp1, tmp100, W, STATUS.
;
DelayW100:
        addlw   -1              ;
        movwf   tmp100          ;
        movlw   d'94'           ; delay balance of first 100 cycles
        call    DelayW1         ;

_d100   movlw   d'96'           ; delay 100 cycles per loop
        call    DelayW1         ;
        decfsz  tmp100,F        ;
          goto  _d100           ;
        return                  ;

; ----------------------------------------------------------------------------
;
; DelayW10k -- Delay { W x 10k } cycles, where W = 2 to 255.
;
;   - Delay includes call/return time (but not load W time).
;   - Registers destroyed: tmp1, tmp100, tmp10k, W, STATUS.
;
DelayW10k:
        addlw   -1              ;
        movwf   tmp10k          ;
        movlw   d'99'           ; delay balance of first 10k cycles
        call    DelayW100       ;
        movlw   d'93'           ;
        call    DelayW1         ;

_d10k   movlw   d'99'           ; delay 10,000 cycles per loop
        call    DelayW100       ;
        movlw   d'95'           ;
        call    DelayW1         ;
        decfsz  tmp10k,F        ;
          goto  _d10k           ;
        return                  ;

; ----------------------------------------------------------------------------
;
; Delay* -- Compact fixed delay functions.
;
;   - Delay includes call/return time.
;   - Registers destroyed: *none* (not even W, not even STATUS).
;   - Warning: multiple levels of call stack are used.
;
Delay80 call    Delay20         ; 4 call levels used
Delay60 call    Delay10         ; 4 call levels used
Delay50 call    Delay10         ; 4 call levels used
Delay40 call    Delay20         ; 4 call levels used
Delay20 call    Delay10         ; 3 call levels used
Delay10 call    Delay4          ; 2 call levels used
Delay6  nop                     ; 1 call level used
Delay5  nop                     ; 1 call level used
Delay4  return                  ; 1 call level used
