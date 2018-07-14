.text
	
.type saved_sp,@object
.comm saved_sp,8,8

#ifdef __aarch64__
.globl start_gthread_asm
.type start_gthread_asm,@function

	//x0 - first argument (new stack)
	//x1 - second argument (function pointer)
	//x2 - third argument (argument
start_gthread_asm:
	//save initial state
	sub sp, sp, #(8*12)
	str x19, [sp, #(8*0)]
	str x20, [sp, #(8*1)]
	str x21, [sp, #(8*2)]
	str x22, [sp, #(8*3)]	
	str x23, [sp, #(8*4)]
	str x24, [sp, #(8*5)]
	str x25, [sp, #(8*6)]
	str x26, [sp, #(8*7)]
	str x27, [sp, #(8*8)]	
	str x28, [sp, #(8*9)]
	str x29, [sp, #(8*10)]
	str x30, [sp, #(8*11)]
	adrp	x9, saved_sp
	mov x10, sp
	str	x10, [x9, #:lo12:saved_sp]
	mov sp, x0
	mov x0, x2
	br x1
	ret


.globl stop_gthread_asm
.type stop_gthread_asm,@function
stop_gthread_asm:
	adrp	x9, saved_sp
	ldr x10, [x9, #:lo12:saved_sp]
	mov sp, x10
	ldr x19, [sp, #(8*0)]
	ldr x20, [sp, #(8*1)]
	ldr x21, [sp, #(8*2)]
	ldr x22, [sp, #(8*3)]	
	ldr x23, [sp, #(8*4)]
	ldr x24, [sp, #(8*5)]
	ldr x25, [sp, #(8*6)]
	ldr x26, [sp, #(8*7)]
	ldr x27, [sp, #(8*8)]	
	ldr x28, [sp, #(8*9)]
	ldr x29, [sp, #(8*10)]
	ldr x30, [sp, #(8*11)]
	add sp, sp, #(8*12)
	ret
	
.globl switch_gthread_asm
.type switch_gthread_asm,@function
//x0 - first argument (current threads state)
//x1 - second argument (next threads state)	
switch_gthread_asm:
	mov x10, sp
	str x10, [x0, #(8*0)]	
	str x19, [x0, #(8*1)]
	str x20, [x0, #(8*2)]
	str x21, [x0, #(8*3)]
	str x22, [x0, #(8*4)]	
	str x23, [x0, #(8*5)]
	str x24, [x0, #(8*6)]
	str x25, [x0, #(8*7)]
	str x26, [x0, #(8*8)]
	str x27, [x0, #(8*9)]	
	str x28, [x0, #(8*10)]
	str x29, [x0, #(8*11)]
	str x30, [x0, #(8*12)]
	
	ldr x10, [x1, #(8*0)]	
	ldr x19, [x1, #(8*1)]
	ldr x20, [x1, #(8*2)]
	ldr x21, [x1, #(8*3)]
	ldr x22, [x1, #(8*4)]	
	ldr x23, [x1, #(8*5)]
	ldr x24, [x1, #(8*6)]
	ldr x25, [x1, #(8*7)]
	ldr x26, [x1, #(8*8)]
	ldr x27, [x1, #(8*9)]	
	ldr x28, [x1, #(8*10)]
	ldr x29, [x1, #(8*11)]
	ldr x30, [x1, #(8*12)]
	mov sp, x10
	ret

.globl switch_and_start_gthread_asm
.type switch_and_start_gthread_asm,@function
//x0 - first argument (current threads state)
//x1 - second argument (new stack)
//x2 - third argument (new function pointer)
//x3 - fourth argumnet (new thread argument)	
switch_and_start_gthread_asm:
	mov x10, sp
	str x10, [x0, #(8*0)]	
	str x19, [x0, #(8*1)]
	str x20, [x0, #(8*2)]
	str x21, [x0, #(8*3)]
	str x22, [x0, #(8*4)]	
	str x23, [x0, #(8*5)]
	str x24, [x0, #(8*6)]
	str x25, [x0, #(8*7)]
	str x26, [x0, #(8*8)]
	str x27, [x0, #(8*9)]	
	str x28, [x0, #(8*10)]
	str x29, [x0, #(8*11)]
	str x30, [x0, #(8*12)]

	mov sp, x1
	mov x0, x3
	br x2
	ret
#endif
	
#ifdef __amd64__
.globl start_gthread_asm
.type start_gthread_asm,@function

	//rdi - first argument (new stack)
	//rsi - second argument (function pointer)
	//rdx - third argument (argument
start_gthread_asm:
	//save initial state
	pushq %r15
	pushq %r14
	pushq %r13
	pushq %r12
	pushq %rbx
	pushq %rbp
	movq %rsp, saved_sp(%rip)
	
	movq %rdi, %rsp
	movq %rdx, %rdi
	callq *%rsi
	ret

.globl stop_gthread_asm
.type stop_gthread_asm,@function
stop_gthread_asm:
	movq saved_sp(%rip), %rsp
	//save initial state
	popq %rbp
	popq %rbx
	popq %r12
	popq %r13
	popq %r14
	popq %r15
	retq
	
.globl switch_gthread_asm
.type switch_gthread_asm,@function
//rdi - first argument (current threads state)
//rsi - second argument (next threads state)	
switch_gthread_asm:
	//save rsp
	movq %rsp,0(%rdi)
	movq %r15,8(%rdi) 
	movq %r14,16(%rdi)
	movq %r13,24(%rdi)	
	movq %r12,32(%rdi)
	movq %rbx,40(%rdi)
	movq %rbp,48(%rdi)	

	//restore
	movq 0(%rsi),%rsp 
	movq 8(%rsi),%r15
	movq 16(%rsi),%r14
	movq 24(%rsi),%r13
	movq 32(%rsi),%r12
	movq 40(%rsi),%rbx
	movq 48(%rsi),%rbp

	retq

.globl switch_and_start_gthread_asm
.type switch_and_start_gthread_asm,@function
//rdi - first argument (current threads state)
//rsi - second argument (new stack)
//rdx - third argument (new function pointer)
//rcx - fourth argumnet (new thread argument)	
switch_and_start_gthread_asm:
	//save rsp
	movq %rsp,0(%rdi)
	movq %r15,8(%rdi) 
	movq %r14,16(%rdi)
	movq %r13,24(%rdi)	
	movq %r12,32(%rdi)
	movq %rbx,40(%rdi)
	movq %rbp,48(%rdi)

	movq %rsi, %rsp
	movq %rcx, %rdi
	callq *%rdx
	retq
#endif
	
.end
