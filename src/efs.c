/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  Minor modifications and note by Andy Sayler (2012) <www.andysayler.com>

  Source: fuse-2.8.7.tar.gz examples directory
  http://sourceforge.net/projects/fuse/files/fuse-2.X/

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall `pkg-config fuse --cflags` fuseefs.c -o fuseefs `pkg-config fuse --libs`

  Note: This implementation is largely stateless and does not maintain
        open file handels between open and release calls (fi->fh).
        Instead, files are opened and closed as necessary inside read(), write(),
        etc calls. As such, the functions that rely on maintaining file handles are
        not implmented (fgetattr(), etc). Those seeking a more efficient and
        more complete implementation may wish to add fi->fh support to minimize
        open() and close() calls and support fh dependent functions.

*/

// solves my problems
#define _GNU_SOURCE

#define FUSE_USE_VERSION 28
#define HAVE_SETXATTR

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include <stdlib.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <limits.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "aes-crypt.h"

typedef struct {
	char* root_dir;
	char* tmp_dir;
} efs_data_t;

#define EFS_DATA ((efs_data_t *) fuse_get_context()->private_data)

static void efs_fullpath(char fpath[PATH_MAX], const char *path)
{
	strncpy(fpath, EFS_DATA->root_dir, PATH_MAX);
	strncat(fpath, path, PATH_MAX);
}

static void efs_temppath(char tpath[PATH_MAX], const char* path)
{
	strncpy(tpath, EFS_DATA->tmp_dir, PATH_MAX);
	strncat(tpath, path, PATH_MAX);
}

static int efs_getattr(const char *path, struct stat *stbuf)
{
	int res;
	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	res = lstat(fpath, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int efs_access(const char *path, int mask)
{
	int res;
	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	res = access(fpath, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int efs_readlink(const char *path, char *buf, size_t size)
{
	int res;

	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	res = readlink(fpath, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}


static int efs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;

	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	dp = opendir(fpath);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0))
			break;
	}

	closedir(dp);
	return 0;
}

static int efs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;
	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		res = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(fpath, mode);
	else
		res = mknod(fpath, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int efs_mkdir(const char *path, mode_t mode)
{
	int res;
	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	res = mkdir(fpath, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int efs_unlink(const char *path)
{
	int res;
	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	res = unlink(fpath);
	if (res == -1)
		return -errno;

	return 0;
}

static int efs_rmdir(const char *path)
{
	int res;
	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	res = rmdir(fpath);
	if (res == -1)
		return -errno;

	return 0;
}

static int efs_symlink(const char *from, const char *to)
{
	int res;
	char ffrom[PATH_MAX];
	char fto[PATH_MAX];
	efs_fullpath(ffrom, from);
	efs_fullpath(fto, to);
	res = symlink(ffrom, fto);
	if (res == -1)
		return -errno;

	return 0;
}

static int efs_rename(const char *from, const char *to)
{
	int res;
	char ffrom[PATH_MAX];
	char fto[PATH_MAX];
	efs_fullpath(ffrom, from);
	efs_fullpath(fto, to);
	res = rename(ffrom, fto);
	if (res == -1)
		return -errno;

	return 0;
}

static int efs_link(const char *from, const char *to)
{
	int res;
	char ffrom[PATH_MAX];
	char fto[PATH_MAX];
	efs_fullpath(ffrom, from);
	efs_fullpath(fto, to);
	res = link(ffrom, fto);
	if (res == -1)
		return -errno;

	return 0;
}

static int efs_chmod(const char *path, mode_t mode)
{
	int res;
	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	res = chmod(fpath, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int efs_chown(const char *path, uid_t uid, gid_t gid)
{
	int res;
	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	res = lchown(fpath, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int efs_truncate(const char *path, off_t size)
{
	int res;
	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	res = truncate(fpath, size);
	if (res == -1)
		return -errno;

	return 0;
}

static int efs_utimens(const char *path, const struct timespec ts[2])
{
	int res;
	struct timeval tv[2];

	tv[0].tv_sec = ts[0].tv_sec;
	tv[0].tv_usec = ts[0].tv_nsec / 1000;
	tv[1].tv_sec = ts[1].tv_sec;
	tv[1].tv_usec = ts[1].tv_nsec / 1000;
	
	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	res = utimes(fpath, tv);
	if (res == -1)
		return -errno;

	return 0;
}

static int efs_open(const char *path, struct fuse_file_info *fi)
{
	int res;
	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	res = open(fpath, fi->flags);
	if (res == -1)
		return -errno;

	close(res);
	return 0;
}

static int efs_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	(void) fi;
	FILE *fp_in, *fp_out;
	int fd;
	int valsize;
	int bytes_read;
	int mode = 0;
	printf("offset: %ld\n", offset);
	
	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);

	char attr[128];
	valsize = getxattr(fpath, "user.encrypted", attr, 128);
	attr[valsize] = '\0';
	if (strcmp(attr, "0") == 0)
	{
		mode = -1;
	}
	
	char tpath[PATH_MAX];
	efs_temppath(tpath, path);

	if (access(tpath, F_OK) == -1)
	{
		fp_in = fopen(fpath, "r");
		fp_out = fopen(tpath, "w");
		do_crypt(fp_in, fp_out, mode, "abc");	
		fclose(fp_in);
		fclose(fp_out);
	}

	fd = open(tpath, O_RDONLY);
	bytes_read = pread(fd, buf, size, offset);
	
	long fs = lseek(fd, 0, SEEK_END);
	close(fd);
	if ((long)(offset+size+1) >= fs)
	{
		remove(tpath);
	}

	return bytes_read;
}

static int efs_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	// printf("size: %d\n", size);
	// printf("offset: %d\n", offset);
	(void) fi;

	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	char tpath[PATH_MAX];
	efs_temppath(tpath, path);

	FILE *fp_in = fopen(fpath, "r");
	FILE *fp_out = fopen(tpath, "w");

	do_crypt(fp_in, fp_out, 0, "abc");

	fclose(fp_in);
	fclose(fp_out);

	int fd = open(tpath, O_WRONLY);
	int res = pwrite(fd, buf, size, offset);

	close(fd);

	fp_in = fopen(tpath, "r");
	fp_out = fopen(fpath, "w");

	do_crypt(fp_in, fp_out, 1, "abc");

	fclose(fp_in);
	fclose(fp_out);

	return res;
	
}

static int efs_statfs(const char *path, struct statvfs *stbuf)
{
	int res;
	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	res = statvfs(fpath, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int efs_create(const char* path, mode_t mode, struct fuse_file_info* fi) {

    (void) fi;
	
	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
    int res;
    res = creat(fpath, mode);
    if(res == -1)
	return -errno;

    close(res);

    return 0;
}


static int efs_release(const char *path, struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) fi;
	return 0;
}

static int efs_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

#ifdef HAVE_SETXATTR
static int efs_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)
{
	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	int res = lsetxattr(fpath, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int efs_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	int res = lgetxattr(fpath, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int efs_listxattr(const char *path, char *list, size_t size)
{
	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	int res = llistxattr(fpath, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int efs_removexattr(const char *path, const char *name)
{
	char fpath[PATH_MAX];
	efs_fullpath(fpath, path);
	int res = lremovexattr(fpath, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations efs_oper = {
	.getattr	= efs_getattr,
	.access		= efs_access,
	.readlink	= efs_readlink,
	.readdir	= efs_readdir,
	.mknod		= efs_mknod,
	.mkdir		= efs_mkdir,
	.symlink	= efs_symlink,
	.unlink		= efs_unlink,
	.rmdir		= efs_rmdir,
	.rename		= efs_rename,
	.link		= efs_link,
	.chmod		= efs_chmod,
	.chown		= efs_chown,
	.truncate	= efs_truncate,
	.utimens	= efs_utimens,
	.open		= efs_open,
	.read		= efs_read,
	.write		= efs_write,
	.statfs		= efs_statfs,
	.create         = efs_create,
	.release	= efs_release,
	.fsync		= efs_fsync,
#ifdef HAVE_SETXATTR
	.setxattr	= efs_setxattr,
	.getxattr	= efs_getxattr,
	.listxattr	= efs_listxattr,
	.removexattr	= efs_removexattr,
#endif
};

#define TMP_DIR "/tmp/efs"

int main(int argc, char *argv[])
{
	umask(0);
	efs_data_t *efs_data;
	efs_data = malloc(sizeof(efs_data_t));
	if (argc < 3)
	{
		printf("error\n");
		return -1;
	}
	efs_data->root_dir = realpath(argv[argc-2], NULL);
	efs_data->tmp_dir = TMP_DIR;
	opendir(TMP_DIR);
	if (errno == ENOENT)
	{
		mkdir(TMP_DIR, 0777);
	}
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;
	return fuse_main(argc, argv, &efs_oper, efs_data);
}