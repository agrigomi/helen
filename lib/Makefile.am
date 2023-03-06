#SUBDIRS = . src include
AUTOMAKE_OPTIONS = foreign subdir-objects
noinst_LIBRARIES = libtools.a
nobase_include_HEADERS = include/context.h \
			include/json.h \
			include/argv.h \
			include/dtype.h \
			include/sig.h \
			include/respawn.h \
			include/trace.h \
			include/err.h
libtools_a_SOURCES = src/context.c \
			src/json.c \
			src/argv.c \
			src/sig.c \
			src/respawn.c \
			src/trace.c
CFLAGS += -Wall -Wextra -Iinclude
