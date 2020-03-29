/* Description of entry in partition table.  */
#ifndef _PARTITION_H
#define _PARTITION_H

// 磁盘里存的分区表结构 16 个字节
// 在磁盘起始地址 0x01BE
struct part_entry {
  // 引导标识 是否是活动分区
  unsigned char bootind;	/* boot indicator 0/ACTIVE_FLAG	 */
  // 开始磁头
  unsigned char start_head;	/* head value for first sector	 */
  // 起始扇区 (0~5) 起始柱面（后两位）
  unsigned char start_sec;	/* sector value + cyl bits for first sector */
  // 起始柱面
  unsigned char start_cyl;	/* track value for first sector	 */
  //  分区类型
   unsigned char sysind;		/* system indicator		 */
  // 结束信息 同起始信息
  unsigned char last_head;	/* head value for last sector	 */
  unsigned char last_sec;	/* sector value + cyl bits for last sector */
  unsigned char last_cyl;	/* track value for last sector	 */
  // 本分区第一个扇区数
  unsigned long lowsec;		/* logical first sector		 */
  // 分区扇区数
  unsigned long size;		/* size of partition in sectors	 */
};

// 活动分区标识
#define ACTIVE_FLAG	0x80	/* value for active in bootind field (hd0) */
// 最多支持分区说
#define NR_PARTITIONS	4	/* number of entries in partition table */
// 分区表起始位置
#define	PART_TABLE_OFF	0x1BE	/* offset of partition table in boot sector */

/* Partition types. */
// 非法分区
#define NO_PART		0x00	/* unused entry */
// minix 分区
#define MINIX_PART	0x81	/* Minix partition type */

#endif /* _PARTITION_H */
