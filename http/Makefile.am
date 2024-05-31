bin_PROGRAMS = http
AUTOMAKE_OPTIONS = foreign subdir-objects
CXXFLAGS += -Wall -Wextra -Isrc -pthread -I../lib/include
LIBS += ../lib/libtools.a
LDFLAGS = -lpthread -lssl -lcrypto -Wl,--copy-dt-needed-entries

nobase_include_HEADERS = src/http.h \
			src/url-codec.h
http_SOURCES = src/main.cpp \
		src/cfg.cpp \
		src/io.cpp \
		src/request.cpp \
		src/response.cpp \
		src/proxy.cpp \
		src/url-codec.cpp \
		src/mime.cpp

