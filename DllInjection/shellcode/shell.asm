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
	mov r12, rsp
	sub rsp, 28h
	and rsp, 0fffffffffffffff0h
	push rbx
	sub rax, 40
	push rax
	mov rcx, rax
	mov rdx, [rax + 16]
	sub rsp, 28h
	call rdx
	add rsp, 28h
	pop rbx
	pop rbx 
	mov rsp, r12
	ret
test_addr:
	mov rax, [rsp]
	sub rax, 5
	ret
min2 ENDP

PUBLIC min2

END