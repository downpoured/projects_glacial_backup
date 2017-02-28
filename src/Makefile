
CC=gcc
CFLAGS=-c -Wall -Werror -std=c11 -Wno-unused-label -Wno-unused-function -Wconversion -D_GNU_SOURCE
LDFLAGS=
SOURCES=dbaccess.c lib_bstrlib.c lib_sphash.c lib_sqlite3.c main.c operations.c tests_sv.c tests_utils.c user_config.c util.c util_archiver.c util_audio_tags.c util_os.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=runbackup.out

debug: CFLAGS += -DDEBUG -D_DEBUG -g
debug: all
ship: CFLAGS += -O3
ship: all

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
	
.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm *.o
	rm runbackup.out



