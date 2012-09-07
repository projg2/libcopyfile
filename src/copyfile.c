/* libcopyfile
 * (c) 2012 Michał Górny
 * Licensed under the terms of the 2-clause BSD license.
 */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "copyfile.h"

#include <stddef.h> /* size_t */
#include <unistd.h>
#include <time.h>
#include <errno.h>

#ifndef COPYFILE_BUFFER_SIZE
#	define COPYFILE_BUFFER_SIZE 4096
#endif

#ifndef COPYFILE_CALLBACK_OPCOUNT
#	define COPYFILE_CALLBACK_OPCOUNT 64
#endif

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
