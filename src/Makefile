
# GlacialBackup is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# GlacialBackup is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

CC=gcc
CFLAGS=-c -Wall  -Werror -std=c11 -Wno-unused-label -Wno-unused-function -Wconversion -D_GNU_SOURCE 
LDFLAGS=
SOURCES=dbaccess.c lib_bstrlib.c lib_sphash.c lib_sqlite3.c operations.c main.c tests_sv.c tests_utils.c user_config.c util.c util_archiver.c util_audio_tags.c util_os.c 
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=glacial_backup.out

debug: CFLAGS += -DDEBUG -D_DEBUG -g
debug: all
ship: CFLAGS += -O3
ship: all

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
	
.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm *.o
	rm glacial_backup.out


