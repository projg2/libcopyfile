/* libcopyfile
 * (c) 2012 Michał Górny
 * Licensed under the terms of the 2-clause BSD license.
 */

#pragma once

#ifndef COPYFILE_H
#define COPYFILE_H 1

#include <sys/types.h>
#include <sys/stat.h>

/**
 * The error return type.
 *
 * It describes the source (syscall) of an error; if not
 * COPYFILE_NO_ERROR, the system errno variable contains the detailed
 * error code.
 */
typedef enum
{
	/**
	 * A special value indicating complete success. It is guaranteed to
	 * evaluate to zero, so you can use the '!' operator to check
	 * function return for success.
	 *
	 * The value of errno is undefined.
	 */
	COPYFILE_NO_ERROR = 0,

	/**
	 * Common error domains. These values indicate the function which
	 * returned an error. The system error code can be found in errno.
	 */
	COPYFILE_ERROR_OPEN_SOURCE,
	COPYFILE_ERROR_OPEN_DEST,
	COPYFILE_ERROR_READ,
	COPYFILE_ERROR_WRITE,
	COPYFILE_ERROR_TRUNCATE,
	COPYFILE_ERROR_READLINK,
	COPYFILE_ERROR_SYMLINK,
	COPYFILE_ERROR_MALLOC,
	COPYFILE_ERROR_STAT,
	COPYFILE_ERROR_MKDIR,
	COPYFILE_ERROR_MKFIFO,
	COPYFILE_ERROR_MKNOD,
	COPYFILE_ERROR_SOCKET,
	COPYFILE_ERROR_BIND,
	COPYFILE_ERROR_XATTR_LIST,
	COPYFILE_ERROR_XATTR_GET,
	COPYFILE_ERROR_XATTR_SET,
	COPYFILE_ERROR_ACL_GET,
	COPYFILE_ERROR_ACL_SET,
	COPYFILE_ERROR_CAP_GET,
	COPYFILE_ERROR_CAP_SET,
	COPYFILE_ERROR_LINK,
	COPYFILE_ERROR_RENAME,
	COPYFILE_ERROR_UNLINK_SOURCE,
	COPYFILE_ERROR_UNLINK_DEST,
	COPYFILE_ERROR_IOCTL_CLONE,
	COPYFILE_ERROR_DOMAIN_MAX,

	/**
	 * An internal error. This should never happen, and is always backed
	 * up with an assert(). If it happens, you should report a bug.
	 */
	COPYFILE_ERROR_INTERNAL = 100,
	/**
	 * The symlink target is longer than we can actually handle. This is
	 * something you should probably investigate and report, since this
	 * means that the readlink() function probably can't read it...
	 * or at least can't fit the length in the return type.
	 */
	COPYFILE_ERROR_SYMLINK_TARGET_TOO_LONG,
	/**
	 * The destination for the unix socket is longer than sockaddr_un
	 * can handle. There is probably nothing we can do about it.
	 */
	COPYFILE_ERROR_SOCKET_DEST_TOO_LONG,
	/**
	 * A particular feature is unsupported or the support is disabled.
	 */
	COPYFILE_ERROR_UNSUPPORTED,

	/**
	 * The operation was aborted by a callback function.
	 */
	COPYFILE_ABORTED = 200,
	/**
	 * A special end-of-file status constant for callback.
	 */
	COPYFILE_EOF,

	COPYFILE_ERROR_MAX
} copyfile_error_t;

/**
 * Constants for metadata copying flags.
 */
typedef enum
{
	/**
	 * Copy the user owner of the file.
	 */
	COPYFILE_COPY_USER = 0x0001,
	/**
	 * Copy the group owner the file.
	 */
	COPYFILE_COPY_GROUP = 0x0002,
	/**
	 * Copy both the user and the group owner of the file.
	 */
	COPYFILE_COPY_OWNER = COPYFILE_COPY_USER | COPYFILE_COPY_GROUP,

	/**
	 * Copy mode (permissions + SUID/SGID/VTX bits) to the file.
	 *
	 * Note that if this is not used but owner of the file is changed,
	 * the resulting mode may be affected by the call to chown().
	 */
	COPYFILE_COPY_MODE = 0x0004,

	/**
	 * Copy file modification time.
	 *
	 * Note that on some systems it is impossible to change mtime
	 * without changing atime. On those systems, this will be equivalent
	 * to COPYFILE_COPY_TIMES.
	 */
	COPYFILE_COPY_MTIME = 0x0008,
	/**
	 * Copy file access time.
	 *
	 * Note that on some systems it is impossible to change atime
	 * without changing mtime. On those systems, this will be equivalent
	 * to COPYFILE_COPY_TIMES.
	 */
	COPYFILE_COPY_ATIME = 0x0010,
	/**
	 * Copy both file access & modification times.
	 */
	COPYFILE_COPY_TIMES = COPYFILE_COPY_MTIME | COPYFILE_COPY_ATIME,

	/**
	 * Copy all supported stat() metadata of a file.
	 */
	COPYFILE_COPY_STAT = COPYFILE_COPY_OWNER | COPYFILE_COPY_MODE
		| COPYFILE_COPY_TIMES,

	/**
	 * Copy regular extended attributes.
	 *
	 * This will omit special attributes, e.g. POSIX ACLs.
	 */
	COPYFILE_COPY_XATTR = 0x0020,
	/**
	 * Copy the ACLs.
	 *
	 * Note that this usually implies copying mode as well (except for
	 * special bits like S_ISUID, S_ISGID and S_ISVTX).
	 */
	COPYFILE_COPY_ACL = 0x0040,
	/**
	 * Copy capabilities.
	 */
	COPYFILE_COPY_CAP = 0x0080,

	/**
	 * All metadata.
	 */
	COPYFILE_COPY_ALL_METADATA = COPYFILE_COPY_STAT
		| COPYFILE_COPY_XATTR | COPYFILE_COPY_ACL | COPYFILE_COPY_CAP
} copyfile_metadata_flag_t;

/**
 * Constants for file types.
 *
 * These constants are always available, even if a particular file type
 * is not supported by the platform.
 */
typedef enum
{
	COPYFILE_REGULAR,
	COPYFILE_SYMLINK,
	COPYFILE_FIFO,
	COPYFILE_CHRDEV,
	COPYFILE_BLKDEV,
	COPYFILE_UNIXSOCK,
	COPYFILE_DIRECTORY,

	/**
	 * A special constant stating that hardlink is being/was created
	 * and thus no file type information was obtained.
	 */
	COPYFILE_HARDLINK,
	/**
	 * A special constant stating that move/rename is being/was done
	 * and thus no file type information was obtained.
	 */
	COPYFILE_MOVE,

	COPYFILE_FILETYPE_MAX
} copyfile_filetype_t;

/**
 * An uniform type for callback progress information.
 */
typedef union
{
	/**
	 * Progress information for regular file (stream) copy.
	 */
	struct
	{
		/**
		 * Current offset in the stream.
		 *
		 * The offset will be calculated from the beginning of copying.
		 * In an EOF case, it will be the actual amount of data copied.
		 */
		off_t offset;
		/**
		 * Apparent file size.
		 *
		 * The size will be the value passed by user or otherwise
		 * obtained from stat() if relevant. It may be 0 if not
		 * provided. It won't be updated in an EOF case, thus it may be
		 * smaller than offset.
		 */
		off_t size;
	} data;

	/**
	 * Symbolic link copying-specific information.
	 */
	union
	{
		/**
		 * Symlink target length, in case of symbolic link copy.
		 *
		 * This is available only in a non-EOF callback. It is a value
		 * passed by user or otherwise obtained from lstat() if relevant.
		 * It may be 0 if not provided. It may be outdated.
		 */
		off_t length;
		/**
		 * Symlink target, in case of symbolic link copy.
		 *
		 * This is available only in an EOF callback.
		 */
		const char* target;
	} symlink;

	/**
	 * Hard-link specific information.
	 *
	 * It is available in both EOF and non-EOF callbacks.
	 */
	struct
	{
		/**
		 * The hard-link target.
		 *
		 * It can be used to obtain more file information whenever
		 * necessary.
		 */
		const char* target;
	} hardlink;

	/**
	 * Move/rename specific information.
	 *
	 * It is available in both EOF and non-EOF callbacks.
	 */
	struct
	{
		/**
		 * The source file path.
		 *
		 * It can be used to obtain more file information whenever
		 * necessary.
		 */
		const char* source;
	} move;

	/**
	 * Device identifier, in case of device file copy.
	 *
	 * This is available in both EOF and non-EOF callbacks.
	 */
	dev_t device;
} copyfile_progress_t;

/**
 * The callback for copyfile_copy_stream().
 *
 * The callback function is called in the following cases:
 * - at the start of copying,
 * - an undefined number of times during the copying (which may depend
 *   on amounts of data copied or function calls),
 * - at the end of copying,
 * - in case of an error.
 *
 * In any of those cases, the @type parameter will hold the file type.
 * The @progress parameter carries detailed progress information,
 * @data carries any user-provided data and @default_return carries
 * the return value which would cause the default behavior (equivalent
 * to one where callback was not defined).
 *
 * At the start of copying and during the process, the callback is
 * called with @state == COPYFILE_NO_ERROR. At the end of file, @state
 * is COPYFILE_EOF. In both cases, @default_return will be 0.
 *
 * In case of an error, @state has any other value; and the system errno
 * variable is set appropriately. Note that the errno may be EINTR as
 * well, which allows the user to interrupt the copy and enter
 * the callback. The @default_return parameter will carry the default
 * way of handling a particular error.
 *
 * The data types available in the @progress union for various file
 * types and states are described in copyfile_progress_t description.
 * In other cases, the contents of the union are undefined.
 *
 * In normal call case, the callback should return 0 in order to
 * continue copying, or a non-zero value to abort it. In the latter
 * case, the function will return with COPYFILE_ABORTED.
 *
 * Please note that in case of an error, callback should return 0
 * in order to retry the operation, and a non-zero value to fail.
 * In that case, the original failure will be returned by function.
 *
 * Also note that the functions don't copy errno before calling
 * the callback on error. If your callback function calls functions
 * which can change errno in that case, you need to either restore its
 * value in the callback or be aware that the errno after the function
 * return will be set in callback.
 */
typedef int (*copyfile_callback_t)(copyfile_error_t state,
		copyfile_filetype_t type, copyfile_progress_t progress,
		void* data, int default_return);

/**
 * Get a textual message for an error.
 *
 * This function does not modify errno.
 *
 * Returns a null-terminated string. If the error code is invalid,
 * an appropriate string will be returned. This function will never
 * return NULL.
 */
const char* copyfile_error_message(copyfile_error_t err);

/**
 * Copy the contents of an input stream onto an output stream.
 *
 * This function takes no special care of the file type -- it just reads
 * the input stream until it reaches EOF, and writes to the output
 * stream.
 *
 * The streams will not be closed. In case of an error, the current
 * offset on both streams is undefined.
 *
 * The @offset_store can be used as a variable holding current offset
 * of stream copy. If it is NULL (0), the function assumes the starting
 * offset is 0. If it is non-NULL, the value of the pointed variable is
 * used as the starting offset. The offset does not affect copying, it
 * is only incremented on writes and passed through to the callback.
 *
 * Additionally, if @offset_store is non-NULL, the final offset value
 * will be written back to the variable on exit. This could be used to
 * obtain the amount of data written in a single call or to support
 * progress reporting on resumed copy.
 *
 * The @expected_size can hold the expected size of the file,
 * or otherwise be 0. It will be only passed through to the callback.
 *
 * If @callback is non-NULL, it will be used to report progress and/or
 * errors. The @callback_data will be passed to it. For more details,
 * see copyfile_callback_t description.
 *
 * If @callback is NULL, default error handling will be used. The EINTR
 * error will be retried indefinitely, and other errors will cause
 * immediate failure.
 *
 * Returns 0 on success, an error otherwise. errno will hold the system
 * error code.
 */
copyfile_error_t copyfile_copy_stream(int fd_in, int fd_out,
		off_t* offset_store, off_t expected_size,
		copyfile_callback_t callback, void* callback_data);

/**
 * Clone the contents of an input stream onto an output stream
 * using Copy-on-Write if possible.
 *
 * The streams will not be closed. In case of an error, the current
 * offset on both streams is undefined.
 *
 * Returns 0 on success, an error otherwise. errno will hold the system
 * error code.
 */
copyfile_error_t copyfile_clone_stream(int fd_in, int fd_out);

/**
 * Copy the contents of a regular file onto a new file.
 *
 * Similarly to 'cp', this function takes no special care of the file
 * type. It just reads @source until EOF, and writes the data to @dest.
 * If @dest exists and is a regular file, it will be replaced. If it is
 * a link to regular file (either hard or symbolic one), the target file
 * will be replaced. It is a named pipe or a special file, the data will
 * be copied into it.
 *
 * The @dest argument has to be a full path to the new file and not
 * just a directory.
 *
 * The @expected_size can hold the expected size of the file,
 * or otherwise be 0. If it's non-zero, the function will try to
 * preallocate a space for the new file.
 *
 * If @callback is non-NULL, it will be used to report progress and/or
 * errors. The @callback_data will be passed to it. For more details,
 * see copyfile_callback_t description.
 *
 * If @callback is NULL, default error handling will be used. The EINTR
 * error will be retried indefinitely, and other errors will cause
 * immediate failure.
 *
 * Returns 0 on success, an error otherwise. errno will hold the system
 * error code.
 */
copyfile_error_t copyfile_copy_regular(const char* source,
		const char* dest, off_t expected_size,
		copyfile_callback_t callback, void* callback_data);

/**
 * Copy the symlink to a new location, preserving the destination.
 *
 * Note that the destination will be preserved without modifications.
 * Especially, relative symlinks will now point relative to the new
 * location.
 *
 * The @dest argument has to be a full path to the new file and not
 * just a directory. It must not exist.
 *
 * If the length of symlink is known, it should be passed
 * as @expected_length. Otherwise, @expected_length should be 0.
 *
 * Returns 0 on success, an error otherwise. errno will hold the system
 * error code.
 */
copyfile_error_t copyfile_copy_symlink(const char* source,
		const char* dest, size_t expected_length,
		copyfile_callback_t callback, void* callback_data);

/**
 * Create a special (incopiable) file.
 *
 * A new file of type @ftype (stat() 'st_mode & S_IFMT' format)
 * at location @path. If @path exists already, the call will fail.
 *
 * If @ftype is S_IFCHR or S_IFBLK (character or block device),
 * the @devid parameter should contain the device number. Otherwise, it
 * can contain any value and will be ignored.
 *
 * If @callback is non-NULL, it will be used to report progress and/or
 * errors. The @callback_data will be passed to it. For more details,
 * see copyfile_callback_t description.
 *
 * Returns 0 on success, an error otherwise. errno will hold the system
 * error code.
 */
copyfile_error_t copyfile_create_special(const char* path, mode_t ftype,
		dev_t devid, copyfile_callback_t callback, void* callback_data);

/**
 * Copy the given file to a new location, preserving its type.
 *
 * If @source is a regular file, the file contents will be copied
 * to the new location. If it is a directory, an empty directory will be
 * created. If it is a symbolic link, the symbolic link will be copied.
 * Otherwise, a new special file of an appropriate type will be created.
 *
 * This is roughly equivalent to 'cp -R' without copying recursively
 * and without any special replacement behavior.
 *
 * The @dest argument has to be a full path to the new file and not
 * just a directory.
 *
 * If the information about the file has been obtained using the lstat()
 * function, a pointer to the obtained structure should be passed
 * as @st. Otherwise, a NULL pointer should be passed.
 *
 * Please note that if the information was obtained using the stat()
 * function instead and if @source is a symbolic link, the underlying
 * file will be copied instead.
 *
 * If @callback is non-NULL, it will be used to report progress and/or
 * errors. The @callback_data will be passed to it. For more details,
 * see copyfile_callback_t description.
 *
 * If @callback is NULL, default error handling will be used. The EINTR
 * error will be retried indefinitely, and other errors will cause
 * immediate failure.
 *
 * Returns 0 on success, an error otherwise. errno will hold the system
 * error code.
 */
copyfile_error_t copyfile_copy_file(const char* source,
		const char* dest, const struct stat* st,
		copyfile_callback_t callback, void* callback_data);

/**
 * Try to clone file to a new location, using atomic clone operation.
 *
 * If @source is a regular file, an atomic clone operation using
 * copyfile_clone_stream() will be attempted. If it fails, stray file
 * may be left over.
 *
 * Otherwise, COPYFILE_ERROR_UNSUPPORTED will be returned.
 *
 * The @dest argument has to be a full path to the new file and not
 * just a directory.
 *
 * If the information about the file has been obtained using the lstat()
 * function, a pointer to the obtained structure should be passed
 * as @st. Otherwise, a NULL pointer should be passed.
 *
 * Please note that if the information was obtained using the stat()
 * function instead and if @source is a symbolic link, the underlying
 * file will be copied instead.
 *
 * Returns 0 on success, an error otherwise. errno will hold the system
 * error code.
 */
copyfile_error_t copyfile_clone_file(const char* source,
		const char* dest, const struct stat* st);

/**
 * Set stat() metadata for a given file.
 *
 * The new metadata should be passed as @st. It must not be NULL. This
 * function assumes that the metadata is exactly fit for the new file,
 * including the file type. If @dest is of other type, incorrect
 * behavior may occur (e.g. if @dest is a symbolic link and @st contains
 * information about a regular file, the symbolic link target
 * permissions may be modified instead).
 *
 * The @flags parameter specifies which properties are to be modified.
 * In order to copy all the stat() metadata, pass 0 (which will imply
 * COPYFILE_COPY_STAT).  For more fine-grained control, see
 * the description of copyfile_metadata_flag_t.
 *
 * Returns a bit-field explaining which operations were done
 * successfully. For each successful change, a flag (from
 * copyfile_metadata_flag_t) will be set.
 */
unsigned int copyfile_set_stat(const char* path,
		const struct stat* st, unsigned int flags);

/**
 * Copy extended attributes of a file.
 *
 * If extended attribute support is disabled, it will return
 * COPYFILE_ERROR_UNSUPPORTED. If source file does not support extended
 * attributes, it will return COPYFILE_NO_ERROR (since there's nothing
 * to copy).
 *
 * Otherwise, it will try hard to copy all the extended attributes.
 * The call will return COPYFILE_NO_ERROR if all attributes were copied
 * correctly, or the first error which occurs. Note that in case
 * of an error other than ENOTSUP (xattr unsupported on destination
 * filesystem), it will still try to copy the remaining attributes.
 *
 * If lstat() data for the file is available, it should be passed
 * as @st. Otherwise, @st should be NULL.
 *
 * Returns 0 on success, an error otherwise. errno will hold the system
 * error code.
 */
copyfile_error_t copyfile_copy_xattr(const char* source,
		const char* dest, const struct stat* st);

/**
 * Copy ACLs of a file.
 *
 * If the ACL support is disabled, it will return
 * COPYFILE_ERROR_UNSUPPORTED. If source file does not support ACLs, it
 * will return COPYFILE_NO_ERROR (since there's nothing to copy).
 *
 * If lstat() data for the file is available, it should be passed
 * as @st. Otherwise, @st should be NULL.
 *
 * Returns 0 on success, an error otherwise. errno will hold the system
 * error code.
 */
copyfile_error_t copyfile_copy_acl(const char* source,
		const char* dest, const struct stat* st);

/**
 * Copy capabilities of a file.
 *
 * If the capability support is disabled, it will return
 * COPYFILE_ERROR_UNSUPPORTED. If source file does not support caps, it
 * will return COPYFILE_NO_ERROR (since there's nothing to copy).
 *
 * If lstat() data for the file is available, it should be passed
 * as @st. Otherwise, @st should be NULL.
 *
 * Returns 0 on success, an error otherwise. errno will hold the system
 * error code.
 */
copyfile_error_t copyfile_copy_cap(const char* source,
		const char* dest, const struct stat* st);

/**
 * Copy common file metadata.
 *
 * This involves copying basic stat() metadata, extended attributes
 * and ACLs.
 *
 * If lstat() result for the source file is available, a pointer to it
 * should be passed as @st. Otherwise, @st should be NULL.
 *
 * The @flags parameter can specify which information should be copied.
 * To copy all available metadata, specify 0 (which results in
 * COPYFILE_COPY_ALL_METADATA). For a more fine-grained choice, take
 * a look at description of copyfile_metadata_flag_t.
 *
 * If @result_flags is not NULL, the bit-field pointed by it will
 * contain a copy of flags explaining which operations were done
 * successfully (it will be reset to zero first).
 *
 * Note that only read/input errors are returned by function. Unless
 * lstat() call fails, the call attempts to copy all metadata even
 * if an error occurs. In case of multiple errors, only the first one
 * is returned. Set/output errors can be distinguished using
 * @result_flags.
 *
 * Returns 0 on success, an error otherwise. errno will hold the system
 * error code.
 */
copyfile_error_t copyfile_copy_metadata(const char* source,
		const char* dest, const struct stat* st,
		unsigned int flags, unsigned int* result_flags);

/**
 * Copy the given file to a new location, preserving given metadata.
 *
 * This calls copyfile_copy_file() and then copyfile_copy_metadata().
 * It is roughly equivalent to 'cp -a' without copying recursively
 * and without any special replacement behavior.
 *
 * The @dest argument has to be a full path to the new file and not
 * just a directory.
 *
 * If the information about the file has been obtained using the lstat()
 * function, a pointer to the obtained structure should be passed
 * as @st. Otherwise, a NULL pointer should be passed.
 *
 * The @flags parameter can specify which metadata should be copied.
 * To copy all available metadata, specify 0 (which results in
 * COPYFILE_COPY_ALL_METADATA). For a more fine-grained choice, take
 * a look at description of copyfile_metadata_flag_t.
 *
 * If @result_flags is not NULL, the bit-field pointed by it will
 * contain a copy of flags explaining which operations were done
 * successfully (it will be reset to zero first).
 *
 * If @callback is non-NULL, it will be used to report progress and/or
 * errors. The @callback_data will be passed to it. For more details,
 * see copyfile_callback_t description.
 *
 * If @callback is NULL, default error handling will be used. The EINTR
 * error will be retried indefinitely, and other errors will cause
 * immediate failure.
 *
 * Returns 0 on success, an error otherwise. errno will hold the system
 * error code.
 */
copyfile_error_t copyfile_archive_file(const char* source,
		const char* dest, const struct stat* st,
		unsigned int flags, unsigned int* result_flags,
		copyfile_callback_t callback, void* callback_data);

/**
 * Hard-link the given file to a new location, fallback to copy.
 *
 * This function should be used whenever a copy of the file is necessary
 * in the new location and it is not intended to be written to.
 *
 * The @source file must not be a directory. The @dest argument has to
 * be a full path to the new file and not just a directory.
 *
 * If @result_flags is not NULL, the bit-field pointed by it will
 * contain a copy of flags explaining which metadata was copied
 * successfully (it will be reset to zero first). If hard-link
 * succeeded, it will be set to COPYFILE_COPY_ALL_METADATA.
 *
 * If @callback is non-NULL, it will be used to report progress and/or
 * errors. The @callback_data will be passed to it. For more details,
 * see copyfile_callback_t description.
 *
 * Note that the '0' callback can be called twice -- once with
 * COPYFILE_HARDLINK and then with another type if link() failed.
 * The COPYFILE_EOF callback will be called only once.
 *
 * The callback will be used to report both link() and copy errors.
 * In case of the former, 0 (retry) return with EXDEV and EPERM will
 * cause the function to try regular copy. The 1 (abort) code will
 * always cause immediate failure.
 *
 * If @callback is NULL, default error handling will be used. The EINTR
 * error will be retried indefinitely, and other errors will cause
 * immediate failure.
 *
 * Returns 0 on success, an error otherwise. errno will hold the system
 * error code.
 */
copyfile_error_t copyfile_link_file(const char* source,
		const char* dest, unsigned int* result_flags,
		copyfile_callback_t callback, void* callback_data);

/**
 * Move the given file to a new location, fallback to copy + unlink.
 *
 * The @source file must not be a directory. The @dest argument has to
 * be a full path to the new file and not just a directory.
 *
 * If the destination file exists and rename() call fails, it will be
 * unlinked and then replaced. The source file will be removed only
 * after successful copy.
 *
 * If @result_flags is not NULL, the bit-field pointed by it will
 * contain a copy of flags explaining which metadata was copied
 * successfully (it will be reset to zero first). If move (rename)
 * succeeded, it will be set to COPYFILE_COPY_ALL_METADATA.
 *
 * If @callback is non-NULL, it will be used to report progress and/or
 * errors. The @callback_data will be passed to it. For more details,
 * see copyfile_callback_t description.
 *
 * Note that the '0' callback can be called twice -- once with
 * COPYFILE_MOVE and then with another type if rename() fails.
 * The COPYFILE_EOF callback will be called only once.
 *
 * The callback will be used to report both rename() and copy errors.
 * In case of the former, 0 (retry) return with EXDEV will cause
 * the function to try regular copy. The 1 (abort) code will always
 * cause immediate failure.
 *
 * If @callback is NULL, default error handling will be used. The EINTR
 * error will be retried indefinitely, and other errors will cause
 * immediate failure.
 *
 * Returns 0 on success, an error otherwise. errno will hold the system
 * error code.
 */
copyfile_error_t copyfile_move_file(const char* source,
		const char* dest, unsigned int* result_flags,
		copyfile_callback_t callback, void* callback_data);

/**
 * Copy the given file to a new location, preserving given metadata.
 * Contents are copied from @dup_copy which is assumed to have the same
 * contents as @source.
 *
 * This is basically equivalent to copyfile_archive_file() except that
 * it provides better means for de-duplication when copying files across
 * filesystems (assuming that @dup_copy and @dest are on the same fs).
 *
 * The @dest argument has to be a full path to the new file and not
 * just a directory.
 *
 * If the information about the file has been obtained using the lstat()
 * function, a pointer to the obtained structure should be passed
 * as @st. Otherwise, a NULL pointer should be passed.
 *
 * The @flags parameter can specify which metadata should be copied.
 * To copy all available metadata, specify 0 (which results in
 * COPYFILE_COPY_ALL_METADATA). For a more fine-grained choice, take
 * a look at description of copyfile_metadata_flag_t.
 *
 * If @result_flags is not NULL, the bit-field pointed by it will
 * contain a copy of flags explaining which operations were done
 * successfully (it will be reset to zero first).
 *
 * If @callback is non-NULL, it will be used to report progress and/or
 * errors. The @callback_data will be passed to it. For more details,
 * see copyfile_callback_t description.
 *
 * If @callback is NULL, default error handling will be used. The EINTR
 * error will be retried indefinitely, and other errors will cause
 * immediate failure.
 *
 * Returns 0 on success, an error otherwise. errno will hold the system
 * error code.
 */
copyfile_error_t copyfile_archive_file_dedup(const char* source,
		const char* dest, const char* dup_copy, const struct stat* st,
		unsigned int flags, unsigned int* result_flags,
		copyfile_callback_t callback, void* callback_data);

/**
 * Hard-link the given file to a new location, fallback to copy.
 * In the latter case, contents are copied from @dup_copy which is
 * assumed to have the same contents as @source.
 *
 * This is basically equivalent to copyfile_link_file() except that
 * it provides better means for de-duplication when copying files across
 * filesystems (assuming that @dup_copy and @dest are on the same fs).
 *
 * The @source file must not be a directory. The @dest argument has to
 * be a full path to the new file and not just a directory.
 *
 * If @result_flags is not NULL, the bit-field pointed by it will
 * contain a copy of flags explaining which metadata was copied
 * successfully (it will be reset to zero first). If hard-link
 * succeeded, it will be set to COPYFILE_COPY_ALL_METADATA.
 *
 * If @callback is non-NULL, it will be used to report progress and/or
 * errors. The @callback_data will be passed to it. For more details,
 * see copyfile_callback_t description.
 *
 * Note that the '0' callback can be called twice -- once with
 * COPYFILE_HARDLINK and then with another type if link() failed.
 * The COPYFILE_EOF callback will be called only once.
 *
 * The callback will be used to report both link() and copy errors.
 * In case of the former, 0 (retry) return with EXDEV and EPERM will
 * cause the function to try regular copy. The 1 (abort) code will
 * always cause immediate failure.
 *
 * If @callback is NULL, default error handling will be used. The EINTR
 * error will be retried indefinitely, and other errors will cause
 * immediate failure.
 *
 * Returns 0 on success, an error otherwise. errno will hold the system
 * error code.
 */
copyfile_error_t copyfile_link_file_dedup(const char* source,
		const char* dest, const char* dup_copy,
		const struct stat* st, unsigned int* result_flags,
		copyfile_callback_t callback, void* callback_data);

/**
 * Move the given file to a new location, fallback to copy + unlink.
 * In the latter case, contents are copied from @dup_copy which is
 * assumed to have the same contents as @source.
 *
 * This is basically equivalent to copyfile_move_file() except that
 * it provides better means for de-duplication when copying files across
 * filesystems (assuming that @dup_copy and @dest are on the same fs).
 *
 * The @source file must not be a directory. The @dest argument has to
 * be a full path to the new file and not just a directory.
 *
 * If the destination file exists and rename() call fails, it will be
 * unlinked and then replaced. The source file will be removed only
 * after successful copy.
 *
 * If @result_flags is not NULL, the bit-field pointed by it will
 * contain a copy of flags explaining which metadata was copied
 * successfully (it will be reset to zero first). If move (rename)
 * succeeded, it will be set to COPYFILE_COPY_ALL_METADATA.
 *
 * If @callback is non-NULL, it will be used to report progress and/or
 * errors. The @callback_data will be passed to it. For more details,
 * see copyfile_callback_t description.
 *
 * Note that the '0' callback can be called twice -- once with
 * COPYFILE_MOVE and then with another type if rename() fails.
 * The COPYFILE_EOF callback will be called only once.
 *
 * The callback will be used to report both rename() and copy errors.
 * In case of the former, 0 (retry) return with EXDEV will cause
 * the function to try regular copy. The 1 (abort) code will always
 * cause immediate failure.
 *
 * If @callback is NULL, default error handling will be used. The EINTR
 * error will be retried indefinitely, and other errors will cause
 * immediate failure.
 *
 * Returns 0 on success, an error otherwise. errno will hold the system
 * error code.
 */
copyfile_error_t copyfile_move_file_dedup(const char* source,
		const char* dest, const char* dup_copy,
		const struct stat* st, unsigned int* result_flags,
		copyfile_callback_t callback, void* callback_data);

#endif /*COPYFILE_H*/
