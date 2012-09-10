/* libcopyfile
 * (c) 2012 Michał Górny
 * Licensed under the terms of the 2-clause BSD license.
 */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "copyfile.h"

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>

#ifndef COPYFILE_BUFFER_SIZE
#	define COPYFILE_BUFFER_SIZE 4096
#endif

#ifndef COPYFILE_CALLBACK_OPCOUNT
#	define COPYFILE_CALLBACK_OPCOUNT 64
#endif

static int not_reached = 0;

copyfile_error_t copyfile_copy_stream(int fd_in, int fd_out,
		copyfile_callback_t callback, void* callback_data)
{
	off_t in_pos = 0;
	char buf[COPYFILE_BUFFER_SIZE];

	int opcount = 0;

	while (1)
	{
		char* bufp = buf;
		ssize_t rd, wr;

		if (callback)
		{
			if (++opcount >= COPYFILE_CALLBACK_OPCOUNT)
			{
				if (callback(COPYFILE_NO_ERROR, in_pos, callback_data))
					return COPYFILE_ABORTED;
				opcount = 0;
			}
		}

		rd = read(fd_in, bufp, sizeof(buf));
		if (rd == -1)
		{
			copyfile_error_t err = COPYFILE_ERROR_READ;

			if (callback
					? !callback(err, in_pos, callback_data)
					: errno == EINTR)
				continue;
			else
				return err;
		}
		else if (rd == 0)
			break;

		in_pos += rd;

		while (rd > 0)
		{
			wr = write(fd_out, bufp, rd);
			if (wr == -1)
			{
				copyfile_error_t err = COPYFILE_ERROR_WRITE;

				if (callback
						? !callback(err, in_pos, callback_data)
						: errno == EINTR)
					continue;
				else
					return err;
			}
			else
			{
				rd -= wr;
				bufp += wr;
			}
		}
	}

	if (callback && callback(COPYFILE_EOF, in_pos, callback_data))
		return COPYFILE_ABORTED;
	return COPYFILE_NO_ERROR;
}

copyfile_error_t copyfile_copy_regular(const char* source,
		const char* dest, off_t expected_size,
		copyfile_callback_t callback, void* callback_data)
{
	int fd_in, fd_out;
#ifdef HAVE_POSIX_FALLOCATE
	int preallocated = 0;
#endif

	fd_in = open(source, O_RDONLY);
	if (fd_in == -1)
		return COPYFILE_ERROR_OPEN_SOURCE;

	fd_out = creat(dest, 0666);
	if (fd_out == -1)
	{
		int hold_errno = errno;

		close(fd_in);

		errno = hold_errno;
		return COPYFILE_ERROR_OPEN_DEST;
	}

#ifdef HAVE_POSIX_FALLOCATE
	if (expected_size && !posix_fallocate(fd_out, 0, expected_size))
		++preallocated;
#endif

	{
		copyfile_error_t ret
			= copyfile_copy_stream(fd_in, fd_out, callback, callback_data);
		int hold_errno = errno;

#ifdef HAVE_POSIX_FALLOCATE
		if (preallocated)
		{
			off_t pos = lseek(fd_out, 0, SEEK_CUR);
			if (pos != -1)
			{
				int trunc_ret = ftruncate(fd_out, pos);
				if (trunc_ret && !ret)
				{
					ret = COPYFILE_ERROR_TRUNCATE;
					hold_errno = trunc_ret;
				}
			}
			else if (!ret)
			{
				ret = COPYFILE_ERROR_TRUNCATE;
				hold_errno = errno;
			}
		}
#endif

		if (close(fd_out) && !ret) /* delayed error? */
			return COPYFILE_ERROR_WRITE;
		close(fd_in);

		errno = hold_errno;
		return ret;
	}
}

static copyfile_error_t try_copy_symlink(const char* source,
		const char* dest, char* buf, ssize_t buf_size)
{
	ssize_t rd = readlink(source, buf, buf_size);

	if (rd == -1)
		return COPYFILE_ERROR_READLINK;

	if (rd < buf_size)
	{
		buf[rd + 1] = 0;

		if (symlink(buf, dest))
			return COPYFILE_ERROR_SYMLINK;
		return COPYFILE_NO_ERROR;
	}

	return COPYFILE_EOF;
}

copyfile_error_t copyfile_copy_symlink(const char* source,
		const char* dest, size_t expected_length)
{
	/* remember that the last cell is for null-terminator */

	/* try to avoid dynamic allocation */
	char buf[COPYFILE_BUFFER_SIZE];
	size_t buf_size = sizeof(buf);

	if (expected_length < buf_size)
	{
		int ret = try_copy_symlink(source, dest, buf, buf_size);
		if (ret != COPYFILE_EOF)
			return ret;

		buf_size *= 2;
	}
	else
		buf_size = expected_length + 1;

	{
		char* buf = 0;
		int ret;

		/* size_t is always unsigned, so -1 safely becomes SIZE_MAX */
		const size_t my_size_max = -1;
		const size_t max_size = my_size_max > SSIZE_MAX
			? SSIZE_MAX : my_size_max;

		do
		{
			char* next_buf = realloc(buf, buf_size);

			if (!next_buf)
			{
				ret = COPYFILE_ERROR_MALLOC;
				if (!buf) /* avoid freeing in the less likely branch */
					return ret;
				break;
			}
			buf = next_buf;

			ret = try_copy_symlink(source, dest, buf, buf_size);

			if (buf_size < max_size / 2)
				buf_size *= 2;
			else
			{
				if (buf_size != max_size)
					buf_size = max_size;
				else
					ret = COPYFILE_ERROR_SYMLINK_TARGET_TOO_LONG;
			}
		}
		while (ret == COPYFILE_EOF);

		free(buf);
		return ret;
	}
}

copyfile_error_t copyfile_copy_file(const char* source,
		const char* dest, const struct stat* st,
		copyfile_callback_t callback, void* callback_data)
{
	struct stat buf;

	if (!st)
	{
		if (lstat(source, &buf))
			return COPYFILE_ERROR_STAT;

		st = &buf;
	}

	switch (st->st_mode & S_IFMT)
	{
		case S_IFREG:
			return copyfile_copy_regular(source, dest, st->st_size,
					callback, callback_data);
		/* XXX: use callback reasonably in other cases */
		case S_IFLNK:
			return copyfile_copy_symlink(source, dest, st->st_size);
		case S_IFDIR:
			if (mkdir(dest, 0777))
				return COPYFILE_ERROR_MKDIR;
			break;
		case S_IFIFO:
			if (mkfifo(dest, 0666))
				return COPYFILE_ERROR_MKFIFO;
			break;
		case S_IFBLK:
		case S_IFCHR:
			if (mknod(dest, (st->st_mode & S_IFMT) | 0666, st->st_rdev))
				return COPYFILE_ERROR_MKNOD;
			break;
		case S_IFSOCK:
			{
				int fd;
				struct sockaddr_un addr;

				const size_t dest_size = strlen(dest) + 1;

				if (dest_size > sizeof(addr.sun_path))
					return COPYFILE_ERROR_SOCKET_DEST_TOO_LONG;

				fd = socket(AF_UNIX, SOCK_STREAM, 0);
				if (fd == -1)
					return COPYFILE_ERROR_SOCKET;

				addr.sun_family = AF_UNIX;
				memcpy(addr.sun_path, dest, dest_size);

				if (bind(fd, &addr, sizeof(addr)))
					return COPYFILE_ERROR_BIND;

				close(fd);
			}
			break;
		default:
			assert(not_reached);
			return COPYFILE_ERROR_INTERNAL;
	}

	return COPYFILE_NO_ERROR;
}
