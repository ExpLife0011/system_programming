;.686
;.MODEL FLAT
;.STACK
;.DATA
.CODE

; ecx - 1 arg
; edx - 2 arg
; eax - return value

min2 PROC
;	mov eax, ecx
;	cmp ecx, edx
;	cmovg eax, edx ; predicative exec
;	lea eax, [eip]
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	call test_addr
	sub rax, 40
;	mov rax, [rax]
	ret
test_addr:
	mov rax, [rsp]
	sub rax, 5
	ret
min2 ENDP

PUBLIC min2

END