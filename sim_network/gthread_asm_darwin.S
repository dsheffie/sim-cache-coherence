.zerofill __DATA,__bss,saved_sp,8,3
	
.section __TEXT,__text

.globl _start_gthread_asm
.p2align        4, 0x90	
	//rdi - first argument (new stack)
	//rsi - second argument (function pointer)
	//rdx - third argument (argument
_start_gthread_asm:
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

.globl _stop_gthread_asm
.p2align        4, 0x90
_stop_gthread_asm:
	movq saved_sp(%rip), %rsp
	//save initial state
	popq %rbp
	popq %rbx
	popq %r12
	popq %r13
	popq %r14
	popq %r15
	retq
	
.globl _switch_gthread_asm
.p2align        4, 0x90
//rdi - first argument (current threads state)
//rsi - second argument (next threads state)	
_switch_gthread_asm:
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

.globl _switch_and_start_gthread_asm
.p2align        4, 0x90
//rdi - first argument (current threads state)
//rsi - second argument (new stack)
//rdx - third argument (new function pointer)
//rcx - fourth argumnet (new thread argument)	
_switch_and_start_gthread_asm:
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
	
.end
