/* libcopyfile
 * (c) 2012 Michał Górny
 * Licensed under the terms of the 2-clause BSD license.
 */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "libcopyfile.h"
#include "common.h"

#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef S_IFSOCK
#	include <sys/socket.h>
#	include <sys/un.h>
#	include <string.h>
#endif

copyfile_error_t copyfile_create_special(const char* path, mode_t ftype,
		dev_t devid, copyfile_callback_t callback, void* callback_data)
{
	copyfile_progress_t progress;
	copyfile_filetype_t cb_ftype;

	switch (ftype)
	{
#ifdef S_IFBLK
		case S_IFBLK:
			progress.device = devid;
			cb_ftype = COPYFILE_BLKDEV;
			break;
#endif /*S_IFBLK*/
#ifdef S_IFCHR
		case S_IFCHR:
			progress.device = devid;
			cb_ftype = COPYFILE_CHRDEV;
			break;
#endif /*S_IFCHR*/
		case S_IFDIR:
			cb_ftype = COPYFILE_DIRECTORY;
			break;
#ifdef S_IFIFO
		case S_IFIFO:
			cb_ftype = COPYFILE_FIFO;
			break;
#endif /*S_IFIFO*/
#ifdef S_IFSOCK
		case S_IFSOCK:
			cb_ftype = COPYFILE_UNIXSOCK;
			break;
#endif /*S_IFSOCK*/
		default:
			assert(not_reached);
			return COPYFILE_ERROR_INTERNAL;
	}

	if (callback && callback(COPYFILE_NO_ERROR, cb_ftype, progress,
				callback_data, 0))
		return COPYFILE_ABORTED;

	while (1)
	{
		int ret;
		copyfile_error_t err;

		switch (ftype)
		{
			case S_IFDIR:
#ifdef _WIN32
				ret = mkdir(path);
#else
				ret = mkdir(path, perm_dir);
#endif
				err = COPYFILE_ERROR_MKDIR;
				break;
#ifdef S_IFIFO
			case S_IFIFO:
#	ifdef HAVE_MKFIFO
				ret = mkfifo(path, perm_file);
				err = COPYFILE_ERROR_MKFIFO;
#	else /*!HAVE_MKFIFO*/
				err = COPYFILE_ERROR_UNSUPPORTED;
#	endif /*HAVE_MKFIFO*/
				break;
#endif /*S_IFIFO*/
#ifdef S_IFBLK
			case S_IFBLK:
#	ifdef HAVE_MKNOD
				ret = mknod(path, ftype | perm_file, devid);
				err = COPYFILE_ERROR_MKNOD;
#	else /*!HAVE_MKNOD*/
				err = COPYFILE_ERROR_UNSUPPORTED;
#	endif /*HAVE_MKNOD*/
				break;
#endif /*S_IFBLK*/
#ifdef S_IFCHR
			case S_IFCHR:
#	ifdef HAVE_MKNOD
				ret = mknod(path, ftype | perm_file, devid);
				err = COPYFILE_ERROR_MKNOD;
#	else /*!HAVE_MKNOD*/
				err = COPYFILE_ERROR_UNSUPPORTED;
#	endif /*HAVE_MKNOD*/
				break;
#endif /*S_IFCHR*/
#ifdef S_IFSOCK
			case S_IFSOCK:
				{
					int fd;
					struct sockaddr_un addr;

					const size_t path_size = strlen(path) + 1;

					if (path_size > sizeof(addr.sun_path))
						return COPYFILE_ERROR_SOCKET_DEST_TOO_LONG;

					fd = socket(AF_UNIX, SOCK_STREAM, 0);
					if (fd != -1)
					{
						addr.sun_family = AF_UNIX;
						memcpy(addr.sun_path, path, path_size);

						ret = bind(fd, (struct sockaddr*) &addr,
								sizeof(addr));
						err = COPYFILE_ERROR_BIND;

						close(fd);
					}
					else
					{
						ret = fd;
						err = COPYFILE_ERROR_SOCKET;
					}
				}
				break;
#endif /*S_IFSOCK*/
			default:
				assert(not_reached);
				return COPYFILE_ERROR_INTERNAL;
		}

		if (!ret)
			break;
		else
		{
			if (!callback || callback(err, cb_ftype, progress,
						callback_data, 1))
				return err;
		}
	}

	if (callback && callback(COPYFILE_EOF, cb_ftype, progress,
				callback_data, 0))
		return COPYFILE_ABORTED;

	return COPYFILE_NO_ERROR;
}
