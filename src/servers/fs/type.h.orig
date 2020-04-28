/* Declaration of the V1 inode as it is on the disk (not in core). */
// 磁盘上v1版的inode
typedef struct {		/* V1.x disk inode */
  mode_t d1_mode;		/* file type, protection, etc. */
  uid_t d1_uid;			/* user id of the file's owner */
  off_t d1_size;		/* current file size in bytes */
  time_t d1_mtime;		/* when was file data last changed */
  u8_t d1_gid;			/* group number */
  u8_t d1_nlinks;		/* how many links to this file */
  u16_t d1_zone[V1_NR_TZONES];	/* block nums for direct, ind, and dbl ind */
} d1_inode;

/* Declaration of the V2 inode as it is on the disk (not in core). */
// v2 和v2 版本inode
typedef struct {		/* V2.x disk inode */
  // 文件保护
  mode_t d2_mode;		/* file type, protection, etc. */
  // 硬连接数量
  u16_t d2_nlinks;		/* how many links to this file. HACK! */
  // 用户所有者
  uid_t d2_uid;			/* user id of the file's owner. */
  // 组id
  u16_t d2_gid;			/* group number HACK! */
  // 　文件大小
  off_t d2_size;		/* current file size in bytes */
  // 问价时间
  time_t d2_atime;		/* when was file data last accessed */
  time_t d2_mtime;		/* when was file data last changed */
  time_t d2_ctime;		/* when was inode data last changed */
  //前７个标注的是数据块
  //倒数第三个是二级块
  // 倒数第二个是三级块
  //  最后一个没有使用
  zone_t d2_zone[V2_NR_TZONES];	/* block nums for direct, ind, and dbl ind */
} d2_inode;
