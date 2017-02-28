
CC=g++
CFLAGS=-c  -Werror -std=c++11 -Wno-unused-label -Wno-unused-function -D_GNU_SOURCE 
LDFLAGS=
SOURCES=main.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=glacial_backup.out

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
	rm glacial_backup.out


