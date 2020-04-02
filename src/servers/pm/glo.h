/* EXTERN should be extern except in table.c */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

/* Global variables. */
// 处理的当前进程
EXTERN struct mproc *mp;	/* ptr to 'mproc' slot of current process */
// 进程表使用数量
EXTERN int procs_in_use;	/* how many processes are marked as IN_USE */
// 引导镜像参数
EXTERN char monitor_params[128*sizeof(char *)];	/* boot monitor parameters */
// 内核信息镜像
EXTERN struct kinfo kinfo;			/* kernel information */

/* The parameters of the call are kept here. */
// 消息处理相关 msg who call_nr
EXTERN message m_in;		/* the incoming message itself is kept here. */
EXTERN int who;			/* caller's proc number */
EXTERN int call_nr;		/* system call number */

// 执行动作数组
extern _PROTOTYPE (int (*call_vec[]), (void) );	/* system call handlers */
extern char core_name[];	/* file name where core images are produced */
// 内核转储信号
EXTERN sigset_t core_sset;	/* which signals cause core images */
// 忽略信号集合
EXTERN sigset_t ign_sset;	/* which signals are by default ignored */

