# Author: J. MUÑOZ
VERSION = 1

# GCC compiler
CC= g++

# Location of Libraries
GLIB = asdfasd
GSTREAMER = asdfasdf
OPENCV = /usr/include/opencv
MATLAB = /opt/matlab2007a/extern/include/


# Linker flags for libraries
LDFLAGS1=$(LDPATHS) -lm -lc -lpthread

# Include paths
INCPATHS =  -I${OPENCV} -I${MATLAB}

#CFLAGS= -g -O3 -Wall -pedantic
CFLAGS= -g -O3 -Wall

# Object files needed for your application
VARIOUSOBJ = emisor.o receptor.o

# C/C++ source
CFILES = emisor.cpp receptor.cpp

# H files
HFILES = emisor.h receptor.h

TARGETS = emisor.o receptor.o sender receiver 


all: ${TARGETS} 

emisor.o: emisor.cpp 
	$(CC) $(CFLAGS) $(INCPATHS) -c emisor.cpp

receptor.o: receptor.cpp 
	$(CC) $(CFLAGS) $(INCPATHS) -c receptor.cpp

sender:	$(VARIOUSOBJ)
	$(CC)  $(CFLAGS) $(LDFLAGS1) -g -o $@ emisor.o

receiver:	$(VARIOUSOBJ)
	$(CC)  $(CFLAGS) $(LDFLAGS1) -g -o $@ receptor.o

clean:
	touch sender receiver
	rm sender receiver
	rm -f *~ *.o ${TARGETS}



