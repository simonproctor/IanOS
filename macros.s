.macro KWRITE_DOUBLE number, row, col
	push %rax
	push %rbx
	push %rcx
	push %rdx
	mov \number, %rdx
	mov $1, %al
	mov $0, %ebx
	mov \row, %bh
	mov \col, %bl
	call Pd
	pop %rdx
	pop %rcx
	pop %rbx
	pop %rax
.endm

.macro KWRITE_STRING str, row, col
	push %rax
	push %rbx
	push %rdx
	mov \str, %rdx
	mov $0, %ebx
	mov \row, %bh
	mov \col, %bl
	call Ps
	pop %rdx
	pop %rbx
	pop %rax
.endm

.macro PUSH_ALL
	push %rax
   	push %rbx
   	push %rcx
   	push %rdx
   	push %rsi
   	push %rdi
   	push %rbp
#   push %r8
#   push %r9
#   push %r10
#   push %r11
#   push %r12
#   push %r13
#   push %r14
#   push %r15
.endm

.macro POP_ALL
#	pop %r15
#	pop %r14
#	pop %r13
#	pop %r12
#	pop %r11
#	pop %r10
#	pop %r9
#	pop %r8
	pop  %rbp
   	pop  %rdi
   	pop  %rsi
   	pop  %rdx
   	pop  %rcx
   	pop  %rbx
   	pop  %rax
.endm

.macro SWITCH_TASKS
	int $20
.endm

.macro SWITCH_TASKS_R15
	int $22
.endm

.macro SEG_DESCRIP name,base,g,segsize,longmode,limit,p,dpl,dt,type
	\name = . - mygdt
	.word	\limit & 0xffff
	.word	\base & 0xffff
	.byte	(\base & 0xff0000) >> 16
	.byte	(\p << 3 + \dpl << 1 + \dt) << 4 + \type
	.byte	(\g << 3 + \segsize << 2 + \longmode << 1) << 4 + ((\limit & 0xf0000) >> 16)
	.byte	(\base & 0xff000000) >> 24
.endm

.macro CODE_SEG_DESCR name,base,limit,dpl
	SEG_DESCRIP \name,\base,1,1,0,\limit,1,\dpl,1,8
.endm

.macro CODE64_SEG_DESCR name,dpl
	SEG_DESCRIP \name, 0, 0, 0, 1, 0, 1, \dpl, 1, 8
.endm

.macro DATA_SEG_DESCR name,base,limit
	SEG_DESCRIP \name,\base,1,1,0,\limit,1,0,1,2
.endm

.macro DATA64_SEG_DESCR name,dpl
	SEG_DESCRIP \name,0,0,0,1,0,1,\dpl,1,2
.endm

.macro TSS_SEG_DESCR64 name,base
	SEG_DESCRIP \name,\base,0,0,0,\tsslength,1,0,0,9
	.quad 0
.endm

