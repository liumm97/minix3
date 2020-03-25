/* This file contains routines for initializing the 8259 interrupt controller:
 *	put_irq_handler: register an interrupt handler
 *	rm_irq_handler: deregister an interrupt handler
 *	intr_handle:	handle a hardware interrupt
 *	intr_init:	initialize the interrupt controller(s)
 */

#include "kernel.h"
#include "proc.h"
#include <minix/com.h>

#define ICW1_AT         0x11	/* edge triggered, cascade, need ICW4 */
#define ICW1_PC         0x13	/* edge triggered, no cascade, need ICW4 */
#define ICW1_PS         0x19	/* level triggered, cascade, need ICW4 */
#define ICW4_AT_SLAVE   0x01	/* not SFNM, not buffered, normal EOI, 8086 */
#define ICW4_AT_MASTER  0x05	/* not SFNM, not buffered, normal EOI, 8086 */
#define ICW4_PC_SLAVE   0x09	/* not SFNM, buffered, normal EOI, 8086 */
#define ICW4_PC_MASTER  0x0D	/* not SFNM, buffered, normal EOI, 8086 */

#define set_vec(nr, addr)	((void)0)

/*===========================================================================*
 *				intr_init				     *
 *===========================================================================*/
PUBLIC void intr_init(mine)
int mine;
{
/* Initialize the 8259s, finishing with all interrupts disabled.  This is
 * only done in protected mode, in real mode we don't touch the 8259s, but
 * use the BIOS locations instead.  The flag "mine" is set if the 8259s are
 * to be programmed for MINIX, or to be reset to what the BIOS expects.
 */
  int i;

  intr_disable();

      /* The AT and newer PS/2 have two interrupt controllers, one master,
       * one slaved at IRQ 2.  (We don't have to deal with the PC that
       * has just one controller, because it must run in real mode.)
       */
    
      // 初始化主芯片icw1
      // ----------------------------------------------
      // | 位7 | 位6 | 位5| 位4| 位3 | 位2 | 位1 | 位0 |
      // ----------------------------------------------
      // | 0   |  0  | 0  | 1  |  LIMI| 0 | SNGL | icw4|
      // ----------------------------------------------
      //    在80x86中 位5、6、7 都是0
      //    limit 1 高位触发 0 边缘触发
      //    SNGL 1-单个芯片 0-级联芯片
      //    icw4 是否使用 icw4
      outb(INT_CTL, machine.ps_mca ? ICW1_PS : ICW1_AT);
      // icw2 
      // 低3位 是 IRQ 其余为中断码
      /* ICW2 for master */
      outb(INT_CTLMASK, mine ? IRQ0_VECTOR : BIOS_IRQ0_VEC);
      // icw3 判断控制器主从关系和关联的芯片脚
      // 0b0000 0100    关联在IRQ2
      outb(INT_CTLMASK, (1 << CASCADE_IRQ));		/* ICW3 tells slaves */
      // cw4 比较复杂 有eio 8086
      outb(INT_CTLMASK, ICW4_AT_MASTER);
      // 屏蔽中断 ocw0
      outb(INT_CTLMASK, ~(1 << CASCADE_IRQ));		/* IRQ 0-7 mask */
      // 同上
      outb(INT2_CTL, machine.ps_mca ? ICW1_PS : ICW1_AT);
      outb(INT2_CTLMASK, mine ? IRQ8_VECTOR : BIOS_IRQ8_VEC);
							/* ICW2 for slave */
      outb(INT2_CTLMASK, CASCADE_IRQ);		/* ICW3 is slave nr */
      outb(INT2_CTLMASK, ICW4_AT_SLAVE);
      outb(INT2_CTLMASK, ~0);				/* IRQ 8-15 mask */

      /* Copy the BIOS vectors from the BIOS to the Minix location, so we
       * can still make BIOS calls without reprogramming the i8259s.
       */
      phys_copy(BIOS_VECTOR(0) * 4L, VECTOR(0) * 4L, 8 * 4L);
}

/*===========================================================================*
 *				put_irq_handler				     *
 *===========================================================================*/
// 向内核注册一个钩子
PUBLIC void put_irq_handler(hook, irq, handler)
irq_hook_t *hook;
int irq;
irq_handler_t handler;
{
/* Register an interrupt handler. */
  int id;
  irq_hook_t **line;

  if (irq < 0 || irq >= NR_IRQ_VECTORS)
      panic("invalid call to put_irq_handler", irq);

  line = &irq_handlers[irq];
  id = 1;
  while (*line != NULL) {
      // 钩子已经注册
      if (hook == *line) return;	/* extra initialization */
      line = &(*line)->next;
      id <<= 1;
  }
  // 如果int 位 32 位 ，不能超过钩子
  if (id == 0) panic("Too many handlers for irq", irq);

  hook->next = NULL;
  hook->handler = handler;
  hook->irq = irq;
  hook->id = id;
  *line = hook;

  // 标记irq 使用
  irq_use |= 1 << irq;
}

/*===========================================================================*
 *				rm_irq_handler				     *
 *===========================================================================*/
PUBLIC void rm_irq_handler(hook)
irq_hook_t *hook;
{
/* Unregister an interrupt handler. */
  int irq = hook->irq; 
  int id = hook->id;
  irq_hook_t **line;

  if (irq < 0 || irq >= NR_IRQ_VECTORS) 
      panic("invalid call to rm_irq_handler", irq);
  // 貌似把所有处理函数都清楚了
  // ??? 
  line = &irq_handlers[irq];
  while (*line != NULL) {
      if ((*line)->id == id) {
          (*line) = (*line)->next;
          if (! irq_handlers[irq]) irq_use &= ~(1 << irq);
          return;
      }
      line = &(*line)->next;
  }
  /* When the handler is not found, normally return here. */
}

/*===========================================================================*
 *				intr_handle				     *
 *===========================================================================*/
PUBLIC void intr_handle(hook)
irq_hook_t *hook;
{
/* Call the interrupt handlers for an interrupt with the given hook list.
 * The assembly part of the handler has already masked the IRQ, reenabled the
 * controller(s) and enabled interrupts.
 */

  /* Call list of handlers for an IRQ. */
  // 调用对应函数
  // 设置对应状态
  while (hook != NULL) {
      /* For each handler in the list, mark it active by setting its ID bit,
       * call the function, and unmark it if the function returns true.
       */
      irq_actids[hook->irq] |= hook->id;
      if ((*hook->handler)(hook)) irq_actids[hook->irq] &= ~hook->id;
      hook = hook->next;
  }

  /* The assembly code will now disable interrupts, unmask the IRQ if and only
   * if all active ID bits are cleared, and restart a process.
   */
}
