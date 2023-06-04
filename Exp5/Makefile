
CC=g++
CFLAGS= -std=c++11 -g
LIBS= -lpthread
TARGET=simpletorrent
SOURCES=src/util.cpp \
        src/bencode.cpp \
				src/sha1.cpp \
				src/shutdown.cpp \
				src/make_tracker_request.cpp \
				src/parse_announce_url.cpp \
        src/parsetorretnfile.cpp \
				src/process_tracker_response.cpp \
				src/simpletorrent.cpp \
		src/pwp.cpp

OBJS=src/util.o \
     src/bencode.o \
		 src/sha1.o\
		 src/shutdown.o \
		 src/make_tracker_request.o \
		 src/parse_announce_url.o \
     src/parsetorrentfile.o \
		 src/process_tracker_response.o \
		 src/simpletorrent.o \
     src/pwp.o

all: ${TARGET}

${TARGET}: ${OBJS}
	${CC} ${CFLAGS} -o bin/${TARGET} ${OBJS} ${LIBS}

%.o: $.c
	$(CC) -c $(CFLAGS) $@ $<

clean:
	rm -rf src/*.o
	rm -rf bin/${TARGET}
	rm -rf src/*.core
	rm -rf *.o
	rm -rf ${TARGET}
	rm -rf *.core

.PHONY: all clean
