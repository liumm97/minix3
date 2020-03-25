# 
! This file, mpx386.s, is included by mpx.s when Minix is compiled for 
! 32-bit Intel CPUs. The alternative mpx88.s is compiled for 16-bit CPUs.

! This file is part of the lowest layer of the MINIX kernel.  (The other part
! is "proc.c".)  The lowest layer does process switching and message handling.
! Furthermore it contains the assembler startup code for Minix and the 32-bit
! interrupt handlers.  It cooperates with the code in "start.c" to set up a 
! good environment for main().

! Every transition to the kernel goes through this file.  Transitions to the 
! kernel may be nested.  The initial entry may be with a system call (i.e., 
! send or receive a message), an exception or a hardware interrupt;  kernel 
! reentries may only be made by hardware interrupts.  The count of reentries 
! is kept in "k_reenter". It is important for deciding whether to switch to 
! the kernel stack and for protecting the message passing code in "proc.c".

! For the message passing trap, most of the machine state is saved in the
! proc table.  (Some of the registers need not be saved.)  Then the stack is
! switched to "k_stack", and interrupts are reenabled.  Finally, the system
! call handler (in C) is called.  When it returns, interrupts are disabled
! again and the code falls into the restart routine, to finish off held-up
! interrupts and run the process or task whose pointer is in "proc_ptr".

! Hardware interrupt handlers do the same, except  (1) The entire state must
! be saved.  (2) There are too many handlers to do this inline, so the save
! routine is called.  A few cycles are saved by pushing the address of the
! appropiate restart routine for a return later.  (3) A stack switch is
! avoided when the stack is already switched.  (4) The (master) 8259 interrupt
! controller is reenabled centrally in save().  (5) Each interrupt handler
! masks its interrupt line using the 8259 before enabling (other unmasked)
! interrupts, and unmasks it after servicing the interrupt.  This limits the
! nest level to the number of lines and protects the handler from itself.

! For communication with the boot monitor at startup time some constant
! data are compiled into the beginning of the text segment. This facilitates 
! reading the data at the start of the boot process, since only the first
! sector of the file needs to be read.

! Some data storage is also allocated at the end of this file. This data 
! will be at the start of the data segment of the kernel and will be read
! and modified by the boot monitor before the kernel starts.

! sections

.sect .text
begtext:
.sect .rom
begrom:
.sect .data
begdata:
.sect .bss
begbss:

#include <minix/config.h>
#include <minix/const.h>
#include <minix/com.h>
#include <ibm/interrupt.h>
#include "const.h"
#include "protect.h"
#include "sconst.h"

/* Selected 386 tss offsets. */
#define TSS3_S_SP0	4

! Exported functions
! Note: in assembly language the .define statement applied to a function name 
! is loosely equivalent to a prototype in C code -- it makes it possible to
! link to an entity declared in the assembly code but does not create
! the entity.

// 相当于预定义
.define	_restart
.define	save

.define	_divide_error
.define	_single_step_exception
.define	_nmi
.define	_breakpoint_exception
.define	_overflow
.define	_bounds_check
.define	_inval_opcode
.define	_copr_not_available
.define	_double_fault
.define	_copr_seg_overrun
.define	_inval_tss
.define	_segment_not_present
.define	_stack_exception
.define	_general_protection
.define	_page_fault
.define	_copr_error

// 中断处理
.define	_hwint00	! handlers for hardware interrupts
.define	_hwint01
.define	_hwint02
.define	_hwint03
.define	_hwint04
.define	_hwint05
.define	_hwint06
.define	_hwint07
.define	_hwint08
.define	_hwint09
.define	_hwint10
.define	_hwint11
.define	_hwint12
.define	_hwint13
.define	_hwint14
.define	_hwint15

// 软中断
.define	_s_call
.define	_p_s_call
.define	_level0_call

! Exported variables.
.define	begbss
.define	begdata

.sect .text
!*===========================================================================*
!*				MINIX					     *
!*===========================================================================*
// 规定的位置 boot程序在完成加载操作系统后 ，会跳转到此处
MINIX:				! this is the entry point for the MINIX kernel
	jmp	over_flags	! skip over the next few bytes
	.data2	CLICK_SHIFT	! for the monitor: memory granularity
flags:
	.data2	0x01FD		! boot monitor flags:
				!	call in 386 mode, make bss, make stack,
				!	load high, don't patch, will return,
				!	uses generic INT, memory vector,
				!	new boot code return
	nop			! extra byte to sync up disassembler
over_flags:

! Set up a C stack frame on the monitor stack.  (The monitor sets cs and ds
! right.  The ss descriptor still references the monitor data segment.)
	// 引导监控程序已经使cpu 进入 保护模式
	// 并且使cs ds 指向内核代码段和数据段
	//  目前ss 指向引导程序 堆栈 ；所以目前使用引导程序堆栈
	// 构造新栈帧
	movzx	esp, sp		! monitor stack is a 16 bit stack
	push	ebp
	mov	ebp, esp
	push	esi
	push	edi
	// ????  应该判断能不用回到引导程序
	cmp	4(ebp), 0	! monitor return vector is
	jz	noret		! nonzero if return possible
	inc	(_mon_return)
	// 把引导程序堆栈指针保存 ，以后恢复使用
noret:	mov	(_mon_sp), esp	! save stack pointer for later return

! Copy the monitor global descriptor table to the address space of kernel and
! switch over to it.  Prot_init() can then update it with immediate effect.

	// _gdt 表示内核预存放GDT表的地方
	// GDT_SELECTO 是规定的 GDT 端描述符
	// 找到目前使用的GDT表线性地址 
    // 把其内容复制到内核_gdt中
	// 一共复制8 个
	sgdt	(_gdt+GDT_SELECTOR)		! get the monitor gdtr
	mov	esi, (_gdt+GDT_SELECTOR+2)	! absolute address of GDT
	mov	ebx, _gdt			! address of kernel GDT
	mov	ecx, 8*8			! copying eight descriptors
copygdt:
	// es:esi -> ds :ebx
	// es 指向监控程序对应段；所以能复制
 eseg	movb	al, (esi)
	movb	(ebx), al
	inc	esi
	inc	ebx
	loop	copygdt
	// 计算_gdt 线性地址 内核数据段基值 + 偏移地址
	mov	eax, (_gdt+DS_SELECTOR+2)	! base of kernel data
	and	eax, 0x00FFFFFF			! only 24 bits
	add	eax, _gdt			! eax = vir2phys(gdt)
	mov	(_gdt+GDT_SELECTOR+2), eax	! set base of GDT
	//  lgdt 寄存器指向目前内核 GDT 表
	lgdt	(_gdt+GDT_SELECTOR)		! switch over to kernel GDT

! Locate boot parameters, set up kernel segment registers and stack.
	// 加载监控程序传来的参数
	mov	ebx, 8(ebp)	! boot parameters offset
	mov	edx, 12(ebp)	! boot parameters length
	mov	eax, 16(ebp)	! address of a.out headers
	mov	(_aout), eax
	// 所有端寄存器切换内核数据段
	// 在minix3 中 ss 和 ds 都是一个物理段
	mov	ax, ds		! kernel data
	mov	es, ax
	mov	fs, ax
	mov	gs, ax
	mov	ss, ax
	// 切换到内核堆栈
	mov	esp, k_stktop	! set sp to point to the top of kernel stack

! Call C startup code to set up a proper environment to run main().
	// 构造start() 参数
	push	edx
	push	ebx
	// ? ? ? 代码上看监控程序使用的数据段就是内核的栈段
	// 没有文档说明
	push	SS_SELECTOR
	push	DS_SELECTOR
	push	CS_SELECTOR
	// 进入start()  函数
	call	_cstart		! cstart(cs, ds, mds, parmoff, parmlen)
	add	esp, 5*4
	// GDT IDL 和进程表都已经基本构造完成

! Reload gdtr, idtr and the segment registers to global descriptor table set
! up by prot_init().

	// 加载对应描述符
	// gdtr 和 idtr 存在线性地址
	// tr 存在一级段地址
	lgdt	(_gdt+GDT_SELECTOR)
	lidt	(_gdt+IDT_SELECTOR)

	jmpf	CS_SELECTOR:csinit
csinit:
    o16	mov	ax, DS_SELECTOR
	mov	ds, ax
	mov	es, ax
	mov	fs, ax
	mov	gs, ax
	mov	ss, ax
	// 装在tr 寄存器
    o16	mov	ax, TSS_SELECTOR	! no other TSS is used
	ltr	ax
	push	0			! set flags to known good state
	popf				! esp, clear nested task and int enable

	jmp	_main			! main()


!*===========================================================================*
!*				interrupt handlers			     *
!*		interrupt handlers for 386 32-bit protected mode	     *
!*===========================================================================*

!*===========================================================================*
!*				hwint00 - 07				     *
!*===========================================================================*
! Note this is a macro, it just looks like a subroutine.
define hwint_master(irq)	
	call	save			/* save interrupted process state */;\
	push	(_irq_handlers+4*irq)	/* irq_handlers[irq]		  */;\
	call	_intr_handle		/* intr_handle(irq_handlers[irq]) */;\
	pop	ecx							    ;\
	cmp	(_irq_actids+4*irq), 0	/* interrupt still active?	  */;\
	jz	0f							    ;\
	inb	INT_CTLMASK		/* get current mask */		    ;\
	orb	al, [1<<irq]		/* mask irq */			    ;\
	outb	INT_CTLMASK		/* disable the irq		  */;\
0:	movb	al, END_OF_INT						    ;\
	outb	INT_CTL			/* reenable master 8259		  */;\
	// 重入 restart1 非重入 restart 
	ret				/* restart (another) process      */

! Each of these entry points is an expansion of the hwint_master macro
	.align	16
_hwint00:		! Interrupt routine for irq 0 (the clock).
	hwint_master(0)

	.align	16
_hwint01:		! Interrupt routine for irq 1 (keyboard)
	hwint_master(1)

	.align	16
_hwint02:		! Interrupt routine for irq 2 (cascade!)
	hwint_master(2)

	.align	16
_hwint03:		! Interrupt routine for irq 3 (second serial)
	hwint_master(3)

	.align	16
_hwint04:		! Interrupt routine for irq 4 (first serial)
	hwint_master(4)

	.align	16
_hwint05:		! Interrupt routine for irq 5 (XT winchester)
	hwint_master(5)

	.align	16
_hwint06:		! Interrupt routine for irq 6 (floppy)
	hwint_master(6)

	.align	16
_hwint07:		! Interrupt routine for irq 7 (printer)
	hwint_master(7)

!*===========================================================================*
!*				hwint08 - 15				     *
!*===========================================================================*
! Note this is a macro, it just looks like a subroutine.
define hwint_slave(irq)	\
	// 下调指令压入栈 ，进入到save
	call	save			/* save interrupted process state */;\
	push	(_irq_handlers+4*irq)	/* irq_handlers[irq]		  */;\
	call	_intr_handle		/* intr_handle(irq_handlers[irq]) */;\
	pop	ecx							    ;\
	cmp	(_irq_actids+4*irq), 0	/* interrupt still active?	  */;\
	jz	0f							    ;\
	inb	INT2_CTLMASK						    ;\
	orb	al, [1<<[irq-8]]					    ;\
	outb	INT2_CTLMASK		/* disable the irq		  */;\
0:	movb	al, END_OF_INT						    ;\
	outb	INT_CTL			/* reenable master 8259		  */;\
	outb	INT2_CTL		/* reenable slave 8259		  */;\
	ret				/* restart (another) process      */

! Each of these entry points is an expansion of the hwint_slave macro
	.align	16
_hwint08:		! Interrupt routine for irq 8 (realtime clock)
	hwint_slave(8)

	.align	16
_hwint09:		! Interrupt routine for irq 9 (irq 2 redirected)
	hwint_slave(9)

	.align	16
_hwint10:		! Interrupt routine for irq 10
	hwint_slave(10)

	.align	16
_hwint11:		! Interrupt routine for irq 11
	hwint_slave(11)

	.align	16
_hwint12:		! Interrupt routine for irq 12
	hwint_slave(12)

	.align	16
_hwint13:		! Interrupt routine for irq 13 (FPU exception)
	hwint_slave(13)

	.align	16
_hwint14:		! Interrupt routine for irq 14 (AT winchester)
	hwint_slave(14)

	.align	16
_hwint15:		! Interrupt routine for irq 15
	hwint_slave(15)

!*===========================================================================*
!*				save					     *
!*===========================================================================*
! Save for protected mode.
! This is much simpler than for 8086 mode, because the stack already points
! into the process table, or has already been switched to the kernel stack.

	.align	16
save:
	cld			! set direction flag to a known value
	pushad			! save "general" registers
    o16	push	ds		! save ds
    o16	push	es		! save es
    o16	push	fs		! save fs
    o16	push	gs		! save gs
	// 硬件从tss 读取ss0并设置 ，所以ss是内核数据段
	mov	dx, ss		! ss is kernel data segment
	mov	ds, dx		! load rest of kernel segments
	mov	es, dx		! kernel does not use fs, gs
	mov	eax, esp	! prepare to return
	incb	(_k_reenter)	! from -1 if not reentering
	jnz	set_restart1	! stack is already kernel stack
	mov	esp, k_stktop
	push	_restart	! build return address for int handler
	xor	ebp, ebp	! for stacktrace
	jmp	RETADR-P_STACKBASE(eax)

	.align	4
set_restart1:
	push	restart1
	jmp	RETADR-P_STACKBASE(eax)

!*===========================================================================*
!*				_s_call					     *
!*===========================================================================*
	.align	16
_s_call:
_p_s_call:
	cld			! set direction flag to a known value
	sub	esp, 6*4	! skip RETADR, eax, ecx, edx, ebx, est
	push	ebp		! stack already points into proc table
	push	esi
	push	edi
    o16	push	ds
    o16	push	es
    o16	push	fs
    o16	push	gs
	mov	dx, ss
	mov	ds, dx
	mov	es, dx
	incb	(_k_reenter)
	mov	esi, esp	! assumes P_STACKBASE == 0
	mov	esp, k_stktop
	xor	ebp, ebp	! for stacktrace
				! end of inline save
				! now set up parameters for sys_call()
	push	ebx		! pointer to user message
	push	eax		! src/dest
	push	ecx		! SEND/RECEIVE/BOTH
	call	_sys_call	! sys_call(function, src_dest, m_ptr)
				! caller is now explicitly in proc_ptr
	mov	AXREG(esi), eax	! sys_call MUST PRESERVE si

! Fall into code to restart proc/task running.

!*===========================================================================*
!*				restart					     *
!*===========================================================================*
_restart:

! Restart the current process or the next process if it is set. 

	// 看看有没有下一个进程
	// 一般中断关闭
	cmp	(_next_ptr), 0		! see if another process is scheduled
	jz	0f
	mov 	eax, (_next_ptr)
	mov	(_proc_ptr), eax	! schedule new process 
	mov	(_next_ptr), 0
	// 切换进程表内进程中断暂存信息的堆栈
0:	mov	esp, (_proc_ptr)	! will assume P_STACKBASE == 0
	lldt	P_LDT_SEL(esp)		! enable process' segment descriptors 
	lea	eax, P_STACKTOP(esp)	! arrange for next interrupt
	// 更改TSS 状态 ，下次中断保存进程信息
	mov	(_tss+TSS3_S_SP0), eax	! to save state in process table
restart1:
	// 中断重入标识
	decb	(_k_reenter)
	// 弹出堆栈信息
	// 对应 stackframe_s
    o16	pop	gs
    o16	pop	fs
    o16	pop	es
    o16	pop	ds
	// Pop EDI, ESI, EBP, EBX, EDX, ECX, and EAX.
	popad
	add	esp, 4		! skip return adr
	// 进程继续运行
	// 内核启动结束 
	// 初始时ss 为0 不知道为什么能运行 ？？？
	iretd			! continue process

!*===========================================================================*
!*				exception handlers			     *
!*===========================================================================*
_divide_error:
	push	DIVIDE_VECTOR
	jmp	exception

_single_step_exception:
	push	DEBUG_VECTOR
	jmp	exception

_nmi:
	push	NMI_VECTOR
	jmp	exception

_breakpoint_exception:
	push	BREAKPOINT_VECTOR
	jmp	exception

_overflow:
	push	OVERFLOW_VECTOR
	jmp	exception

_bounds_check:
	push	BOUNDS_VECTOR
	jmp	exception

_inval_opcode:
	push	INVAL_OP_VECTOR
	jmp	exception

_copr_not_available:
	push	COPROC_NOT_VECTOR
	jmp	exception

_double_fault:
	push	DOUBLE_FAULT_VECTOR
	jmp	errexception

_copr_seg_overrun:
	push	COPROC_SEG_VECTOR
	jmp	exception

_inval_tss:
	push	INVAL_TSS_VECTOR
	jmp	errexception

_segment_not_present:
	push	SEG_NOT_VECTOR
	jmp	errexception

_stack_exception:
	push	STACK_FAULT_VECTOR
	jmp	errexception

_general_protection:
	push	PROTECTION_VECTOR
	jmp	errexception

_page_fault:
	push	PAGE_FAULT_VECTOR
	jmp	errexception

_copr_error:
	push	COPROC_ERR_VECTOR
	jmp	exception

!*===========================================================================*
!*				exception				     *
!*===========================================================================*
! This is called for all exceptions which do not push an error code.

	.align	16
exception:
 sseg	mov	(trap_errno), 0		! clear trap_errno
 sseg	pop	(ex_number)
	jmp	exception1

!*===========================================================================*
!*				errexception				     *
!*===========================================================================*
! This is called for all exceptions which push an error code.

	.align	16
errexception:
 sseg	pop	(ex_number)
 sseg	pop	(trap_errno)
exception1:				! Common for all exceptions.
	push	eax			! eax is scratch register
	mov	eax, 0+4(esp)		! old eip
 sseg	mov	(old_eip), eax
	movzx	eax, 4+4(esp)		! old cs
 sseg	mov	(old_cs), eax
	mov	eax, 8+4(esp)		! old eflags
 sseg	mov	(old_eflags), eax
	pop	eax
	call	save
	push	(old_eflags)
	push	(old_cs)
	push	(old_eip)
	push	(trap_errno)
	push	(ex_number)
	call	_exception		! (ex_number, trap_errno, old_eip,
					!	old_cs, old_eflags)
	add	esp, 5*4
	ret

!*===========================================================================*
!*				level0_call				     *
!*===========================================================================*
_level0_call:
	call	save
	jmp	(_level0_func)

!*===========================================================================*
!*				data					     *
!*===========================================================================*

.sect .rom	! Before the string table please
	.data2	0x526F		! this must be the first data entry (magic #)

.sect .bss
k_stack:
	.space	K_STACK_BYTES	! kernel stack
k_stktop:			! top of kernel stack
	.comm	ex_number, 4
	.comm	trap_errno, 4
	.comm	old_eip, 4
	.comm	old_cs, 4
	.comm	old_eflags, 4
