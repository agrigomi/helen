bin_PROGRAMS = hl
AUTOMAKE_OPTIONS = foreign subdir-objects
CXXFLAGS = -Wall -Wextra -Isrc -pthread -I../lib/include -I/usr/local/include
LIBS = ../lib/libtools.a
LDFLAGS = -L/usr/local/lib/ -lpthread -lssl -lcrypto
nobase_include_HEADERS = src/cfg.h
hl_SOURCES = src/main.cpp \
	src/cfg.cpp \
	src/ssl.cpp

