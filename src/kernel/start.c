/* This file contains the C startup code for Minix on Intel processors.
 * It cooperates with mpx.s to set up a good environment for main().
 *
 * This code runs in real mode for a 16 bit kernel and may have to switch
 * to protected mode for a 286.
 * For a 32 bit kernel this already runs in protected mode, but the selectors
 * are still those given by the BIOS with interrupts disabled, so the
 * descriptors need to be reloaded and interrupt descriptors made.
 */

#include "kernel.h"
#include "protect.h"
#include "proc.h"
#include <stdlib.h>
#include <string.h>

FORWARD _PROTOTYPE( char *get_value, (_CONST char *params, _CONST char *key));
/*===========================================================================*
 *				cstart					     *
 *===========================================================================*/
PUBLIC void cstart(cs, ds, mds, parmoff, parmsize)
U16_t cs, ds;			/* kernel code and data segment */
U16_t mds;			/* monitor data segment */
U16_t parmoff, parmsize;	/* boot parameters offset and length */
{
/* Perform system initializations prior to calling main(). Most settings are
 * determined with help of the environment strings passed by MINIX' loader.
 */
  char params[128*sizeof(char *)];		/* boot monitor parameters */
  register char *value;				/* value in key=value pair */
  // 编译器定义的变量；用以界定边界
  extern int etext, end;

  /* Decide if mode is protected; 386 or higher implies protected mode.
   * This must be done first, because it is needed for, e.g., seg2phys().
   * For 286 machines we cannot decide on protected mode, yet. This is 
   * done below. 
   *
   */
// 32 位进入保护模式
#if _WORD_SIZE != 2
  machine.protected = 1;	
#endif

  /* Record where the kernel and the monitor are. */
  // 存放的都是线性地址（物理地址）
  // etext、_ etext是编译链接器预定义的外部变量，表示正文段结束后的第一个地址（正文段长）
  // edata、_ edata则表示已初始化数据段长 
  // end、_ end表示未初始化的bss结束后的第一个地址（数据段长）。
  kinfo.code_base = seg2phys(cs);
  kinfo.code_size = (phys_bytes) &etext;	/* size of code segment */
  kinfo.data_base = seg2phys(ds);
  kinfo.data_size = (phys_bytes) &end;		/* size of data segment */

  /* Initialize protected mode descriptors. */
  // 初始化GDT
  prot_init();

  /* Copy the boot parameters to the local buffer. */
  // 复制引导参数到内核
  kinfo.params_base = seg2phys(mds) + parmoff;
  kinfo.params_size = MIN(parmsize,sizeof(params)-2);
  phys_copy(kinfo.params_base, vir2phys(params), kinfo.params_size);

  /* Record miscellaneous information for user-space servers. */
  kinfo.nr_procs = NR_PROCS;
  kinfo.nr_tasks = NR_TASKS;
  strncpy(kinfo.release, OS_RELEASE, sizeof(kinfo.release));
  kinfo.release[sizeof(kinfo.release)-1] = '\0';
  strncpy(kinfo.version, OS_VERSION, sizeof(kinfo.version));
  kinfo.version[sizeof(kinfo.version)-1] = '\0';
  kinfo.proc_addr = (vir_bytes) proc;
  // 也就是数据段地址
  kinfo.kmem_base = vir2phys(0);
  kinfo.kmem_size = (phys_bytes) &end;	

  /* Processor?  86, 186, 286, 386, ... 
   * Decide if mode is protected for older machines. 
   */
  // 设置计算机结构
  machine.processor=atoi(get_value(params, "processor")); 
#if _WORD_SIZE == 2
  machine.protected = machine.processor >= 286;		
#endif
  if (! machine.protected) mon_return = 0;

  /* XT, AT or MCA bus? */
  value = get_value(params, "bus");
  if (value == NIL_PTR || strcmp(value, "at") == 0) {
      machine.pc_at = TRUE;			/* PC-AT compatible hardware */
  } else if (strcmp(value, "mca") == 0) {
      machine.pc_at = machine.ps_mca = TRUE;	/* PS/2 with micro channel */
  }

  /* Type of VDU: */
  value = get_value(params, "video");		/* EGA or VGA video unit */
  if (strcmp(value, "ega") == 0) machine.vdu_ega = TRUE;
  if (strcmp(value, "vga") == 0) machine.vdu_vga = machine.vdu_ega = TRUE;

  /* Return to assembler code to switch to protected mode (if 286), 
   * reload selectors and call main().
   */
}

/*===========================================================================*
 *				get_value				     *
 *===========================================================================*/

PRIVATE char *get_value(params, name)
_CONST char *params;				/* boot monitor parameters */
_CONST char *name;				/* key to look up */
{
/* Get environment value - kernel version of getenv to avoid setting up the
 * usual environment array.
 */
  register _CONST char *namep;
  register char *envp;

  for (envp = (char *) params; *envp != 0;) {
	for (namep = name; *namep != 0 && *namep == *envp; namep++, envp++)
		;
	if (*namep == '\0' && *envp == '=') return(envp + 1);
	while (*envp++ != 0)
		;
  }
  return(NIL_PTR);
}
