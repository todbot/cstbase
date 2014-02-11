


TARGET = cstbase-timeset

# try to do some autodetecting
UNAME := $(shell uname -s)

ifeq "$(UNAME)" "Darwin"
LIBS += ../cstbase-lib/cstbase-lib.a
LIBS += -framework IOKit -framework CoreFoundation
EXE=
endif

ifeq "$(OS)" "Windows_NT"
LIBS += ../cstbase-lib/cstbase-lib.a
LIBS += -lsetupapi -Wl,--enable-auto-import -static-libgcc -static-libstdc++ 
EXE=.exe
endif


INCLUDES += -I. -I../cstbase-lib -I../cstbase-lib/hidapi
INCLUDES += -DUSE_HIDDATA

CFLAGS  += -g -Wall $(INCLUDES)

# -----------------------------

.PHONY: default all clean

default: $(TARGET)
all: default

$(TARGET):
	$(CC) $(CFLAGS) $(TARGET).c $(LIBS)  -o $(TARGET)$(EXE) 


clean:
	-rm -f *.o
	-rm -f $(TARGET)$(EXE)
