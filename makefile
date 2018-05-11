# GNU MAKE Makefile for GPS ephem functions
#  (use 'gmake' for BSD,  and probably 'gmake CLANG=Y')
#
# Usage: make [CLANG=Y] [XCOMPILE=Y] [MSWIN=Y] [tgt]
#
# where tgt can be any of:
# [test_gps|clean]
#
#	'XCOMPILE' = cross-compile for Windows,  using MinGW,  on a Linux or BSD box
#	'MSWIN' = compile for Windows,  using MinGW,  on a Windows machine
#	'CLANG' = use clang instead of GCC;  BSD/Linux only
# None of these: compile using g++ on BSD or Linux
#	Note that I've only tried clang on PC-BSD (which is based on FreeBSD).

CC=g++
EXE=
CURL=-lcurl
CFLAGS=-Wextra -Wall -O3 -pedantic -Wno-unused-parameter -I $(INSTALL_DIR)/include

# You can have your include files in ~/include and libraries in
# ~/lib,  in which case only the current user can use them;  or
# (with root privileges) you can install them to /usr/local/include
# and /usr/local/lib for all to enjoy.

ifdef GLOBAL
	INSTALL_DIR=/usr/local
else
	INSTALL_DIR=~
endif

ifdef CLANG
	CC=clang
endif

RM=rm -f

# I'm using 'mkdir -p' to avoid error messages if the directory exists.
# It may fail on very old systems,  and will probably fail on non-POSIX
# systems.  If so,  change to '-mkdir' and ignore errors.

ifdef MSWIN
	EXE=.exe
	MKDIR=-mkdir
else
	MKDIR=mkdir -p
endif

LIB_DIR=$(INSTALL_DIR)/lib
LIBSADDED=-L $(LIB_DIR)

ifdef XCOMPILE
	CC=x86_64-w64-mingw32-g++
	EXE=.exe
	LIB_DIR=$(INSTALL_DIR)/win_lib
	LIBSADDED=-L $(LIB_DIR) -mwindows
endif

all: names$(EXE) test_gps$(EXE) list_gps$(EXE) list_gps.cgi

install:
	$(MKDIR) $(INSTALL_DIR)/include
	cp gps.h   $(INSTALL_DIR)/include

uninstall:
	rm -f $(INSTALL_DIR)/include/gps.h

.cpp.o:
	$(CC) $(CFLAGS) -c $<

clean:
	$(RM) gps.o names.o names$(EXE) test_gps.o test_gps$(EXE) list_gps.o list_gps$(EXE) list_gps.cgi

names$(EXE): names.o
	$(CC) $(CFLAGS) -o names$(EXE) names.o $(LIBSADDED) -llunar

test_gps$(EXE): test_gps.o gps.o
	$(CC) $(CFLAGS) -o test_gps$(EXE) test_gps.o gps.o $(LIBSADDED) -llunar $(CURL) -lm -lsatell

list_gps$(EXE): list_gps.cpp gps.o
	$(CC) $(CFLAGS) -o list_gps$(EXE) list_gps.cpp gps.o $(LIBSADDED) -llunar $(CURL) -lm -lsatell

list_gps.cgi  : list_cgi.cpp list_gps.cpp gps.o
	$(CC) $(CFLAGS) -o list_gps.cgi list_cgi.cpp -DCGI_VERSION list_gps.cpp gps.o $(LIBSADDED) -llunar $(CURL) -lm -lsatell

gps.o: gps.cpp
	$(CC) $(CFLAGS) $(CURLI) -c $<

dailyize: dailyize.c
	$(CC) $(CFLAGS) -o dailyize dailyize.c
