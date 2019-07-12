
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
CFLAGS=-c -Wall  -Werror -std=c11 -Wno-format-zero-length -Wno-unused-label -Wno-unused-function -Wconversion -D_GNU_SOURCE 
LDFLAGS=-lm
SOURCES=dbaccess.c lib_bstrlib.c lib_sphash.c lib_sqlite3.c operations.c  \
user_config.c user_interface.c util.c util_archiver.c util_audio_tags.c util_higher.c util_files.c util_os.c \
tests/tests.c tests/tests.h tests/tests_array_utils.c tests/tests_dbaccess.c \
tests/tests_os.c tests/tests_userconfig.c tests/tests_util.c tests/tests_util_archiver.c \
tests/tests_util_audio_tags.c tests/whole_db.c tests/whole_operation.c


OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=glacial_backup.out

debug: CFLAGS += -DDEBUG -D_DEBUG -g
debug: all
ship: CFLAGS += -O3
ship: all

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	
.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm *.o
	rm tests/*.o
	rm glacial_backup.out

