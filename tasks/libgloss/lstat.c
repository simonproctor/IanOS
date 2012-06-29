#include <sys/stat.h>
#include <errno.h>
#include "filesystem.h"

#undef errno
extern int errno;

int lstat(char *path, struct stat *st)
{
	struct FileInfo inf;
	sys_lstat(path, &inf);
	st->st_mode = S_IFCHR;
	st->st_ino = inf.inode;
  	st->st_size = inf.Length;
  	st->st_atime = inf.atime;
  	st->st_ctime = inf.ctime;
  	st->st_mtime = inf.mtime;
  	errno = 0;
  	return 0;
}
