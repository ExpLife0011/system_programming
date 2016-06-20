;.686
;.MODEL FLAT
;.STACK
;.DATA
.CODE

; ecx - 1 arg
; edx - 2 arg
; eax - return value

funcAsm PROC
	mov r12, 48h ;H
	mov r13, 55h ;U
	mov r14, 49h ;I
	ret
funcAsm ENDP

PUBLIC funcAsm

END