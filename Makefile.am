ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = util/copyfile
lib_LTLIBRARIES = src/libcopyfile.la
include_HEADERS = src/libcopyfile.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = data/libcopyfile.pc

src_libcopyfile_la_SOURCES = \
	src/copyfile-copy-stream.c \
	src/copyfile-clone-stream.c \
	src/copyfile-copy-regular.c \
	src/copyfile-copy-symlink.c \
	src/copyfile-create-special.c \
	src/copyfile-copy-file.c \
	src/copyfile-clone-file.c \
	src/copyfile-set-stat.c \
	src/copyfile-copy-xattr.c \
	src/copyfile-copy-acl.c \
	src/copyfile-copy-cap.c \
	src/copyfile-copy-metadata.c \
	src/copyfile-archive-file.c \
	src/copyfile-link-file.c \
	src/copyfile-move-file.c \
	src/copyfile-archive-file-dedup.c \
	src/copyfile-link-file-dedup.c \
	src/copyfile-move-file-dedup.c \
	src/copyfile-error-message.c \
	src/common.h src/libcopyfile.h
src_libcopyfile_la_LDFLAGS = -no-undefined -version-info 0:0:0

util_copyfile_SOURCES = \
	util/copyfile.c
util_copyfile_CPPFLAGS = -I$(top_srcdir)/src
util_copyfile_LDADD = src/libcopyfile.la

EXTRA_DIST = NEWS
NEWS: configure.ac Makefile.am
	git for-each-ref refs/tags --sort '-*committerdate' \
		--format '# %(tag) (%(*committerdate:short))%0a%(contents:body)' \
		> NEWS
