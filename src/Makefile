
CC=gcc
CFLAGS=-c -Werror -std=c11 -Wno-unused-label -Wno-unused-function -D_GNU_SOURCE 
LDFLAGS=
SOURCES=main.c testmacros.c
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
