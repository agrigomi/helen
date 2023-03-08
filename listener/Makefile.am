bin_PROGRAMS = hl
AUTOMAKE_OPTIONS = foreign subdir-objects
CXXFLAGS += -Wall -Wextra -Isrc -pthread -I../lib/include
LIBS += ../lib/libtools.a
LDFLAGS += -lpthread -lstdc++ -lssl -lcrypto
nobase_include_HEADERS = src/cfg.h
hl_SOURCES = src/main.cpp \
	src/cfg.cpp

