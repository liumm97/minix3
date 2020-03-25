#ifndef TYPE_H
#define TYPE_H

typedef _PROTOTYPE( void task_t, (void) );

/* Process table and system property related types. */ 
typedef int proc_nr_t;			/* process table entry number */
typedef short sys_id_t;			/* system process index */
typedef struct {			/* bitmap for system indexes */
  bitchunk_t chunk[BITMAP_CHUNKS(NR_SYS_PROCS)];
} sys_map_t;

struct boot_image {
  proc_nr_t proc_nr;			/* process number to use */
  task_t *initial_pc;			/* start function for tasks */
  int flags;				/* process flags */
  unsigned char quantum;		/* quantum (tick count) */
  int priority;				/* scheduling priority */
  int stksize;				/* stack size for tasks */
  short trap_mask;			/* allowed system call traps */
  bitchunk_t ipc_to;			/* send mask protection */
  long call_mask;			/* system call protection */
  char proc_name[P_NAME_LEN];		/* name in process table */
};

struct memory {
  phys_clicks base;			/* start address of chunk */
  phys_clicks size;			/* size of memory chunk */
};

/* The kernel outputs diagnostic messages in a circular buffer. */
struct kmessages {
  int km_next;				/* next index to write */
  int km_size;				/* current size in buffer */
  char km_buf[KMESS_BUF_SIZE];		/* buffer for messages */
};

struct randomness {
  struct {
	int r_next;				/* next index to write */
	int r_size;				/* number of random elements */
	unsigned short r_buf[RANDOM_ELEMENTS]; /* buffer for random info */
  } bin[RANDOM_SOURCES];
};

#if (CHIP == INTEL)
typedef unsigned reg_t;		/* machine register */

/* The stack frame layout is determined by the software, but for efficiency
 * it is laid out so the assembly code to use it is as simple as possible.
 * 80286 protected mode and all real modes use the same frame, built with
 * 16-bit registers.  Real mode lacks an automatic stack switch, so little
 * is lost by using the 286 frame for it.  The 386 frame differs only in
 * having 32-bit registers and more segment registers.  The same names are
 * used for the larger registers to avoid differences in the code.
 */
// 中断时保存进程状态所有信息
struct stackframe_s {           /* proc_ptr points here */
#if _WORD_SIZE == 4
  u16_t gs;                     /* last item pushed by save */
  u16_t fs;                     /*  ^ */
#endif
  u16_t es;                     /*  | */
  u16_t ds;                     /*  | */
  reg_t di;			/* di through cx are not accessed in C */
  reg_t si;			/* order is to match pusha/popa */
  reg_t fp;			/* bp */
  reg_t st;			/* hole for another copy of sp */
  reg_t bx;                     /*  | */
  reg_t dx;                     /*  | */
  reg_t cx;                     /*  | */
  reg_t retreg;			/* ax and above are all pushed by save */
  reg_t retadr;			/* return address for assembly code save() */
  // 指令计数器 cs:ip = cs:pc
  reg_t pc;			/*  ^  last item pushed by interrupt */
  reg_t cs;                     /*  | */
  reg_t psw;                    /*  | */
  reg_t sp;                     /*  | */
  reg_t ss;                     /* these are pushed by CPU during interrupt */
};

// 在386 保护模式中 段描述符
// P,present位,1表示所描述的段存在(有效),为0表示所描述的段无效,使用该描述符会引起异常
//DPL,Descriptor privilege,描述符特权级别,说明所描述段的特权级别
//DT,描述符类型位,1说明当前描述符为存储段描述符,0为系统描述符或门描述符.
//
//TYPE:
//位0:A(accessed)位,表明描述符是否已被访问;把选择子装入段寄存器时,该位被标记为1
//位3:E(EXECUTABLE?)位,0说明所描述段为数据段;1为可执行段(代码段)
//
//当为数据段时,
   //位1为W位,说明该数据段是否可写(0只读,1可写)
   //位2为ED位,说明该段的扩展方向(0向高位扩展,1向低位扩展)
//当为可执行段是,
   //位1为R位,说明该执行段是否可读(0只执行,1可读)
   //位2为C位,0说明该段不是一致码段(普通代码段),1为一致码段
struct segdesc_s {		/* segment descriptor for protected mode */
  u16_t limit_low;
  u16_t base_low;
  u8_t base_middle;
  u8_t access;			/* |P|DL|1|X|E|R|A| */ 
  u8_t granularity;		/* |G|X|0|A|LIMT| */
  u8_t base_high;
};

typedef unsigned long irq_policy_t;	
typedef unsigned long irq_id_t;	

// 中断处理中使用
// 一个中断可以对应多个硬件设备 ；所以是链表
typedef struct irq_hook {
  struct irq_hook *next;		/* next hook in chain */
  int (*handler)(struct irq_hook *);	/* interrupt handler */
  int irq;				/* IRQ vector number */ 
  int id;				/* id of this hook */
  int proc_nr;				/* NONE if not in use */
  irq_id_t notify_id;			/* id to return on interrupt */
  irq_policy_t policy;			/* bit mask for policy */
} irq_hook_t;

typedef int (*irq_handler_t)(struct irq_hook *);

#endif /* (CHIP == INTEL) */

#if (CHIP == M68000)
/* M68000 specific types go here. */
#endif /* (CHIP == M68000) */

#endif /* TYPE_H */
