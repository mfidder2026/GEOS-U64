; GEOS KERNAL extension for the Commodore 64 Ultimate / Ultimate 64
;
; Network (TCP/UDP sockets) and turbo control through the Ultimate
; Command Interface (UCI) at $DF1C-$DF1F.
;
; Protocol: https://1541u-documentation.readthedocs.io (Command Interface)
; and https://github.com/xlar54/ultimateii-dos-lib (network target 3).
;
; All calls are made through the jump table at UltJumpTable ($FE50).
; Every call returns X = error code (0 = ok, DEV_NOT_FOUND = no UCI,
; NET_ERROR = the Ultimate reported an error, see UltGetStatus) with
; the Z flag set from X, so "bnex error" works after each call.
; r0-r3 may be destroyed unless noted. Strings are plain ASCII.

.include "const.inc"
.include "geossym.inc"
.include "geosmac.inc"
.include "config.inc"
.include "kernal.inc"
.include "c64.inc"

.ifdef ultimateNet

.global UltDetect
.global UltSetSpeed

UCI_CONTROL     = $df1c ; write: control / read: status
UCI_COMMAND     = $df1d ; write: command data / read: ID
UCI_RESPONSE    = $df1e ; read: response data
UCI_STATUS      = $df1f ; read: status data

UCI_DEVICEID    = $c9

UCI_PUSH        = (1 << 0)
UCI_ACCEPT      = (1 << 1)
UCI_ABORT       = (1 << 2)
UCI_ERROR       = (1 << 3)
UCI_STATEMASK   = ((1 << 4) | (1 << 5))
UCI_STATE_IDLE  = 0
UCI_STATE_BUSY  = (1 << 4)
UCI_STATUS_AV   = (1 << 6)
UCI_DATA_AV     = (1 << 7)

TARGET_NETWORK  = $03

NET_GET_IFCOUNT = $02
NET_GET_IPADDR  = $05
NET_TCP_CONNECT = $07
NET_UDP_CONNECT = $08
NET_CLOSE       = $09
NET_READ        = $10
NET_WRITE       = $11

NET_ERROR       = $50   ; not used by GEOS 2.0 itself

WRITE_CHUNK     = 255   ; payload bytes per NET_WRITE command (max. 893)

TURBO_CTRL      = $d031 ; U64 speed register (bits 0-3, bit 7 badlines off)

.segment "uci"

;---------------------------------------------------------------
; internal helpers
;---------------------------------------------------------------

; bank in I/O; the previous configuration is restored by uci_exit
uci_enter:
	pha
	lda CPU_DATA
	sta savedCPU
	LoadB CPU_DATA, IO_IN
	pla
	rts

uci_exit_ok:
	ldx #0
uci_exit:
	pha
	lda savedCPU
	sta CPU_DATA
	pla
	cpx #0
	rts

; start a network command: Y = command (uci_start_a: A = target).
; Returns with C=0; if there is no UCI it does not return but leaves the
; API call with DEV_NOT_FOUND. Afterwards, parameters are written to
; UCI_COMMAND.
uci_start:
	lda #TARGET_NETWORK
uci_start_a:
	sta uciTarget
	lda UCI_COMMAND
	cmp #UCI_DEVICEID
	bne @none
	lda #UCI_ERROR
	bit UCI_CONTROL
	beq @1
	sta UCI_CONTROL         ; clear a pending error
@1:	lda #UCI_STATEMASK
	bit UCI_CONTROL
	beq @2
	LoadB UCI_CONTROL, UCI_ABORT
	lda #UCI_STATEMASK
@w:	bit UCI_CONTROL
	bne @w
@2:	MoveB uciTarget, UCI_COMMAND
	sty UCI_COMMAND
	clc
	rts
@none:	pla                     ; drop our return address
	pla
uci_nodev:
	ldx #DEV_NOT_FOUND
	jmp uci_exit

; push the command, wait for it, read the status channel into
; UltStatusBuf. Returns X = 0 or NET_ERROR.
; The data channel must be drained afterwards (uci_get/uci_drain).
uci_exec:
	ldx #NET_ERROR
	lda #UCI_ERROR
	bit UCI_CONTROL
	bne @rts
	LoadB UCI_CONTROL, UCI_PUSH
@w1:	bit UCI_CONTROL         ; wait for push acknowledge
	bne @w1
@w2:	lda UCI_CONTROL         ; wait while busy
	and #UCI_STATEMASK
	cmp #UCI_STATE_BUSY
	beq @w2
	ldy #0
@st:	lda #UCI_STATUS_AV
	bit UCI_CONTROL
	beq @3
	lda UCI_STATUS
	cpy #UltStatusLen-1
	bcs @st
	sta UltStatusBuf,y
	iny
	bne @st
@3:	lda #0
	sta UltStatusBuf,y
	; no status or "00..." means success
	tax                     ; X = 0
	cpy #0
	beq @rts
	lda UltStatusBuf
	cmp #'0'
	bne @err
	lda UltStatusBuf+1
	cmp #'0'
	beq @rts
@err:	ldx #NET_ERROR
@rts:	rts

; read one data byte into A; C=1 when there is no more data
; (the command is then acknowledged and the UCI is idle)
uci_get:
	lda #UCI_DATA_AV
	bit UCI_CONTROL
	beq @next
	lda UCI_RESPONSE
	clc
	rts
@next:	LoadB UCI_CONTROL, UCI_ACCEPT
@w1:	bit UCI_CONTROL
	bne @w1
@w2:	lda UCI_CONTROL
	and #UCI_STATEMASK
	cmp #UCI_STATE_BUSY
	beq @w2
	cmp #UCI_STATE_IDLE
	bne uci_get             ; another data packet follows
	sec
	rts

; skip remaining data, preserves X
uci_drain:
	jsr uci_get
	bcc uci_drain
	rts

; run command Y with one parameter byte in r2L, drain the data and
; return the first data byte in A (0 if none)
uci_simple1:
	jsr uci_start
	MoveB r2L, UCI_COMMAND
	bcc uci_run             ; always
; run command Y without parameters
uci_simple0:
	jsr uci_start
uci_run:
	jsr uci_exec
	jsr uci_get
	bcs @1
	pha
	jsr uci_drain
	pla
	jmp uci_exit
@1:	lda #0
	jmp uci_exit

;---------------------------------------------------------------
; public API
;---------------------------------------------------------------

; UltDetect: is the command interface there?
; out: X = 0 and A = number of network interfaces, or X = DEV_NOT_FOUND
;      r0 = pointer to the turbo state: (r0) = $D030, (r0)+1 = $D031
_UltDetect:
	jsr uci_enter
	MoveB clkreg, turboState
	MoveB TURBO_CTRL, turboState+1
	LoadW r0, turboState
	ldy #NET_GET_IFCOUNT
	bne uci_simple0         ; always

; NetGetIP: in: A = interface (0 = first), r0 = 12 byte buffer
; out: buffer = IP, netmask, gateway (4 bytes each)
_NetGetIP:
	jsr uci_enter
	sta r2L
	ldy #NET_GET_IPADDR
	jsr uci_start
	MoveB r2L, UCI_COMMAND
	jsr uci_exec
	ldy #0
@1:	jsr uci_get
	bcs @2
	cpy #12
	bcs @1
	sta (r0),y
	iny
	bne @1
@2:	jmp uci_exit

; NetTCPConnect / NetUDPConnect
; in:  r0 = host name or dotted IP, 0-terminated (max 255 chars)
;      r1 = port
; out: A = socket number
_NetUDPConnect:
	ldy #NET_UDP_CONNECT
	bne connect             ; always
_NetTCPConnect:
	ldy #NET_TCP_CONNECT
connect:
	jsr uci_enter
	jsr uci_start
	MoveB r1L, UCI_COMMAND
	MoveB r1H, UCI_COMMAND
	ldy #0
@1:	lda (r0),y
	sta UCI_COMMAND
	beq @2
	iny
	bne @1
@2:	jmp uci_run

; NetClose: in: A = socket
_NetClose:
	jsr uci_enter
	sta r2L
	ldy #NET_CLOSE
	jmp uci_simple1

; NetRead: in:  A = socket, r0 = buffer, r1 = max. bytes (1-1472)
;          out: r1 = bytes read, 0 = connection closed (socket is then
;               freed, no NetClose needed), $FFFF = nothing received
;               yet (the Ultimate waits 40 ms; try again later)
;          r0 is advanced by 256 for every full page received
_NetRead:
	jsr uci_enter
	sta r2L
	ldy #NET_READ
	jsr uci_start
	MoveB r2L, UCI_COMMAND
	MoveB r1L, UCI_COMMAND
	MoveB r1H, UCI_COMMAND
	jsr uci_exec
	LoadW r1, 0
	jsr uci_get             ; 16 bit length first
	bcs @done
	sta r1L
	jsr uci_get
	bcs @done
	sta r1H
	; with a length header the call succeeded: "02,NO DATA" ($FFFF) and
	; "socket closed" (0) are states reported in r1, not errors
	ldx #0
	ldy #0
@1:	jsr uci_get
	bcs @done
	sta (r0),y
	iny
	bne @1
	inc r0H
	bne @1                  ; always
@done:	jmp uci_exit

; NetWrite: in: A = socket, r0 = data, r1 = length
_NetWrite:
	jsr uci_enter
	sta r2L
@chunk:	lda r1L
	ora r1H
	beq @done
	; r3L = bytes in this chunk = min(r1, WRITE_CHUNK)
	lda #WRITE_CHUNK
	ldx r1H
	bne @1
	cmp r1L
	bcc @1
	lda r1L
@1:	sta r3L
	ldy #NET_WRITE
	jsr uci_start
	MoveB r2L, UCI_COMMAND
	ldy #0
@2:	lda (r0),y
	sta UCI_COMMAND
	iny
	cpy r3L
	bne @2
	jsr uci_exec
	jsr uci_drain
	txa
	bne @err
	AddB r3L, r0L
	bcc @3
	inc r0H
@3:	lda r1L
	sec
	sbc r3L
	sta r1L
	bcs @chunk
	dec r1H
	jmp @chunk
@done:	ldx #0
@err:	jmp uci_exit

; UltCommand: any command interface command (DOS target 1, control
; target 4, ...). in:  r0 = command bytes: target, command, parameters
;                       r1 = their length (2-896)
;                       r2 = reply buffer, r3L = its size (max. 255)
;                  out: r1L = reply bytes stored, status via UltGetStatus
_UltCommand:
	jsr uci_enter
	ldy #0
	lda (r0),y              ; target
	pha
	iny
	lda (r0),y              ; command
	tay
	pla
	jsr uci_start_a
	ldy #2                  ; send bytes 2 .. r1-1
@send:	cpy r1L
	bne @1
	lda r1H
	beq @exec
@1:	lda (r0),y
	sta UCI_COMMAND
	iny
	bne @send
	inc r0H                 ; next page
	dec r1H
	jmp @send
@exec:	jsr uci_exec
	ldy #0
@get:	jsr uci_get
	bcs @done
	cpy r3L
	bcs @get                ; buffer full: skip the rest
	sta (r2),y
	iny
	bne @get
@done:	sty r1L
	jmp uci_exit

; NetListen, NetListenStop, NetListenState, NetListenSocket: the
; firmware has no listener commands ($12-$15 in ultimateii-dos-lib),
; so the jump table entries stay reserved and return X = NET_ERROR.
_NetNotSupported:
	lda #0
	sta UltStatusBuf        ; no status text
	ldx #NET_ERROR
	rts

; UltSetSpeed: in: A = $D031 value (bits 0-3 speed, bit 7 badlines off)
; Needs Turbo Control = "TurboEnable Bit" or "U64 Turbo Registers".
_UltSetSpeed:
	jsr uci_enter
	ldx TURBO_CTRL
	inx
	beq @none               ; reads $FF: no turbo registers
	sta TURBO_CTRL
	LoadB clkreg, 1         ; turbo enable bit on
	jmp uci_exit_ok
@none:	jmp uci_nodev

; UltGetStatus: out: r0 = status text of the last command (0-terminated)
_UltGetStatus:
	LoadW r0, UltStatusBuf
	ldx #0
	rts

UltStatusLen = 32
UltStatusBuf:
	.res UltStatusLen
savedCPU:
	.byte 0
uciTarget:
	.byte 0
turboState:
	.word 0

;---------------------------------------------------------------
; jump table at $FE50, fixed for applications
;---------------------------------------------------------------
.segment "ultjmp"

UltDetect:      jmp _UltDetect          ; $FE50
NetGetIP:       jmp _NetGetIP           ; $FE53
NetTCPConnect:  jmp _NetTCPConnect      ; $FE56
NetUDPConnect:  jmp _NetUDPConnect      ; $FE59
NetClose:       jmp _NetClose           ; $FE5C
NetRead:        jmp _NetRead            ; $FE5F
NetWrite:       jmp _NetWrite           ; $FE62
NetListen:      jmp _NetNotSupported    ; $FE65
NetListenStop:  jmp _NetNotSupported    ; $FE68
NetListenState: jmp _NetNotSupported    ; $FE6B
NetListenSocket:jmp _NetNotSupported    ; $FE6E
UltSetSpeed:    jmp _UltSetSpeed        ; $FE71
UltGetStatus:   jmp _UltGetStatus       ; $FE74
UltCommand:     jmp _UltCommand         ; $FE77
	; signature for applications: "UNET", API version, reserved
	.byte "UNET", 2, 0              ; $FE7A (version 2: UltCommand)

.endif ; ultimateNet
