#
# Makefile for "cstbase-lib" and "cstbase-tool"
# should work on Mac OS X, Windows, Linux, and other Linux-like systems
#  
# Build arguments:
# - "OS=macosx"  -- build Mac version on Mac OS X
# - "OS=windows" -- build Windows version on Windows
# - "OS=linux"   -- build Linux version on Linux
# 
# Architecture is usually detected automatically, so normally just type "make"
#
# Dependencies: 
# - hidapi (included), which uses libusb on Linux-like OSes
#
# Platform-specific notes:
#
# Mac OS X 
#   - Install Xcode with "Unix Dev Support" and "Command-line tools" (in Xcode Preferences)
#   - make
#
# Windows XP/7  
#   - Install MinGW and MSYS (http://www.tdragon.net/recentgcc/ )
#   - make
#
# Linux (Ubuntu) 
#   - apt-get install build-essential pkg-config libusb-1.0-0-dev 
#   - make
#
# FreeBSD
#   - libusb is part of the OS so no pkg-config needed.
#   - No -ldl on FreeBSD necessary.
#   - For FreeBSD versions < 10, iconv is a package that needs to be installed;
#     in this case it lives in /usr/local/lib/
#
# Linux Ubuntu 32-bit cross-compile on 64-bit
#   To build 32-bit on 64-bit Ubuntu, try a chrooted build:
#   (warning this will use up a lot of disk space)
#   - sudo apt-get install ubuntu-dev-tools
#   - pbuilder-dist oneiric i386 create
#   - mkdir $HOME/i386
#   - cp -r cstbase $HOME/i386
#   - pbuilder-dist oneiric i386 login --bindmounts $HOME/i386
#     (now in the chrooted area)
#   - apt-get install libusb-1.0-0 libusb-1.0-0-dev
#   - cd $HOME/i386/cstbase
#   - CFLAGS='-I/usr/include/libusb-1.0' LIBS='-lusb-1.0' make
#   - exit
#   
# Raspberry Pi
#   - apt-get install libusb-1.0.0-dev
#   - make
#
# BeagleBone / BeagleBoard (on Angstrom Linux)
#   - opkg install libusb-0.1-4-dev  (FIXME: uses HIDAPI & libusb-1.0 now)	
#   - May need to symlink libusb 
#      cd /lib; ln -s libusb-0.1.so.4 libusb.so
#   - make
#
#

# try to do some autodetecting
UNAME := $(shell uname -s)

ifeq "$(UNAME)" "Darwin"
	OS=macosx
endif

ifeq "$(OS)" "Windows_NT"
	OS=windows
endif

ifeq "$(UNAME)" "Linux"
	OS=linux
endif

ifeq "$(UNAME)" "FreeBSD"
	OS=freebsd
endif

ifeq "$(PKGOS)" ""
   PKGOS = $(OS)
endif

# pick low-level implemenation style when invoking make, like:
#   % USBLIB_TYPE=HIDDATA make
# 
# "HIDAPI" type is best for Mac, Windows, Linux Desktop, 
#  but has dependencies on iconv, libusb-1.0, pthread, dl
#
# "HIDDATA" type is best for low-resource Linux, 
#  and the only dependencies it has is libusb-0.1
#

USBLIB_TYPE ?= HIDAPI

# uncomment for debugging HID stuff
#CFLAGS += -DDEBUG_PRINTF


#################  Mac OS X  ##################################################
ifeq "$(OS)" "macosx"
LIBTARGET = libCstbase.dylib
CFLAGS += -mmacosx-version-min=10.6

ifeq "$(USBLIB_TYPE)" "HIDAPI"
CFLAGS += -DUSE_HIDAPI
CFLAGS += -arch i386 -arch x86_64
# don't need pthread with clang
#CFLAGS += -pthread
CFLAGS += -I./hidapi/hidapi 
OBJS = ./hidapi/mac/hid.o
endif

ifeq "$(USBLIB_TYPE)" "HIDDATA"
CFLAGS += -DUSE_HIDDATA
OBJS = ./hiddata.o
OPT_HOME := /opt/local/bin
CFLAGS += `$(OPT_HOME)/libusb-config --cflags`
LIBS   += `$(OPT_HOME)/libusb-config --libs`
endif

LIBS += -framework IOKit -framework CoreFoundation

EXEFLAGS = 
#LIBFLAGS = -bundle -o $(LIBTARGET) -Wl,-search_paths_first $(LIBS)
LIBFLAGS = -dynamiclib -o $(LIBTARGET) -Wl,-search_paths_first $(LIBS)
EXE=

endif

#################  Windows  ##################################################
ifeq "$(OS)" "windows"
LIBTARGET = cstbase-lib.dll
#LIBS +=  -mwindows -lsetupapi -Wl,--enable-auto-import -static-libgcc -static-libstdc++ -lkernel32 
#LIBS +=  -mwindows -lsetupapi -Wl,-Bdynamic -lgdi32 -Wl,--enable-auto-import -static-libgcc -static-libstdc++ -lkernel32
LIBS +=             -lsetupapi -Wl,--enable-auto-import -static-libgcc -static-libstdc++ 

ifeq "$(USBLIB_TYPE)" "HIDAPI"
CFLAGS += -DUSE_HIDAPI
CFLAGS += -I./hidapi/hidapi 
OBJS = ./hidapi/windows/hid.o
endif

ifeq "$(USBLIB_TYPE)" "HIDDATA"
CFLAGS += -DUSE_HIDDATA
OBJS = ./hiddata.o
endif

EXEFLAGS =
#LIBFLAGS = -shared -o $(LIBTARGET) -Wl,--add-stdcall-alias -Wl,--export-all-symbols -Wl,--out-implib,$(LIBTARGET).a $(LIBS)
LIBFLAGS = -shared -o $(LIBTARGET) -Wl,--add-stdcall-alias -Wl,--export-all-symbols
EXE= .exe

endif

#################  Linux  ####################################################
ifeq "$(OS)" "linux"
LIBTARGET = cstbase-lib.so

ifeq "$(USBLIB_TYPE)" "HIDAPI"
CFLAGS += -DUSE_HIDAPI
CFLAGS += -I./hidapi/hidapi 
OBJS = ./hidapi/libusb/hid.o
CFLAGS += `pkg-config libusb-1.0 --cflags` -fPIC
LIBS   += `pkg-config libusb-1.0 --libs` -lrt -lpthread -ldl
endif

ifeq "$(USBLIB_TYPE)" "HIDDATA"
CFLAGS += -DUSE_HIDDATA
OBJS = ./hiddata.o
CFLAGS += `pkg-config libusb --cflags` -fPIC
LIBS   += `pkg-config libusb --libs` 
endif

EXEFLAGS = -static
LIBFLAGS = -shared -o $(LIBTARGET) $(LIBS)
EXE=

endif

################  FreeBSD  ###################################################
ifeq "$(OS)" "freebsd"
LIBTARGET = cstbase-lib.so

ifeq "$(USBLIB_TYPE)" "HIDAPI"
CFLAGS += -DUSE_HIDAPI
CFLAGS += -I./hidapi/hidapi 
OBJS = ./hidapi/libusb/hid.o
CFLAGS += -I/usr/local/include -fPIC
LIBS   += -lusb -lrt -lpthread
ifndef FBSD10
LIBS   += -L/usr/local/lib -liconv
endif
endif

ifeq "$(USBLIB_TYPE)" "HIDDATA"
CFLAGS += -DUSE_HIDDATA
OBJS = ./hiddata.o
CFLAGS += -I/usr/local/include -fPIC
LIBS   += -L/usr/local/lib -lusb 
endif

# Static binaries don't play well with the iconv implementation of FreeBSD 10
ifndef FBSD10
EXEFLAGS = -static
endif
LIBFLAGS = -shared -o $(LIBTARGET) $(LIBS)
EXE=

endif

#################  WRT Linux  ################################################
ifeq "$(OS)" "wrt"
LIBTARGET = cstbase-lib.so

# HIDAPI build doesn't work, use HIDDATA instead
ifeq "$(USBLIB_TYPE)" "HIDAPI"
CFLAGS += -DUSE_HIDAPI
CFLAGS += -I./hidapi/hidapi 
OBJS = ./hidapi/libusb/hid.o
CFLAGS += `pkg-config libusb-1.0 --cflags` -fPIC 
LIBS   += `pkg-config libusb-1.0 --libs` -lrt -lpthread -ldl 
endif

ifeq "$(USBLIB_TYPE)" "HIDDATA"
CFLAGS += -DUSE_HIDDATA $(COPT_FLAGS)
OBJS = ./hiddata.o
LIBS += $(LDOPT_FLAGS) 
#LIBS += $(STAGING_DIR)/usr/lib/libusb.a 
#can't build this static for some reason
LIBS += -lusb
endif

#EXEFLAGS = -static
LIBFLAGS = -shared -o $(LIBTARGET) $(LIBS)
EXE=

endif

##############  Cross-compile WRT Linux  #####################################
ifeq "$(OS)" "wrtcross"

ifeq "$(USBLIB_TYPE)" "HIDDATA"
CFLAGS += -DUSE_HIDDATA
OBJS = ./hiddata.o

WRT_SDK_HOME := $(HOME)/projects/openwrt/sdk/OpenWrt-SDK-ar71xx-for-Linux-i686-gcc-4.3.3+cs_uClibc-0.9.30.1
CC = $(WRT_SDK_HOME)/staging_dir/toolchain-mips_r2_gcc-4.3.3+cs_uClibc-0.9.30.1/usr/bin/mips-openwrt-linux-gcc
LD = $(WRT_SDK_HOME)/staging_dir/toolchain-mips_r2_gcc-4.3.3+cs_uClibc-0.9.30.1/usr/bin/mips-openwrt-linux-ld
CFLAGS += "-I$(WRT_SDK_HOME)/staging_dir/target-mips_r2_uClibc-0.9.30.1/usr/include" -fPIC
LIBS   += "$(WRT_SDK_HOME)/staging_dir/target-mips_r2_uClibc-0.9.30.1/usr/lib/libusb.a"
#LDFLAGS += -static

endif

EXEFLAGS = -static
LIBFLAGS = -shared -o $(LIBTARGET) $(LIBS)
EXE=

endif



#####################  Common  ###############################################

#CFLAGS += -O -Wall -std=gnu99 -I ../hardware/firmware 
CFLAGS += -std=gnu99 
CFLAGS += -g

OBJS +=  cstbase-lib.o 

#all: msg cstbase-tool cstbase-server-simple
all: msg cstbase-tool lib

msg: 
	@echo "building for OS=$(OS)"

# symbolic targets:
help:
	@echo "This Makefile works on multiple archs. Use one of the following:"
	@echo "make OS=windows ... build Windows  cstbase-lib and cstbase-tool" 
	@echo "make OS=linux   ... build Linux    cstbase-lib and cstbase-tool" 
	@echo "make OS=freebsd ... build FreeBSD    cstbase-lib and cstbase-tool" 
	@echo "make OS=macosx  ... build Mac OS X cstbase-lib and cstbase-tool" 
	@echo "make OS=wrt     ... build OpenWrt cstbase-lib and cstbase-tool"
	@echo "make USBLIB_TYPE=HIDDATA OS=linux ... build using low-dep method"
	@echo "make lib        ... build cstbase-lib shared library"
	@echo "make package PKGOS=mac  ... zip up build, give it a name 'mac' "
	@echo "make clean ..... to delete objects and hex file"
	@echo

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

cstbase-tool: $(OBJS) cstbase-tool.o
	$(CC) $(CFLAGS) -c cstbase-tool.c -o cstbase-tool.o
	$(CC) $(CFLAGS) $(EXEFLAGS) -g $(OBJS) $(LIBS) cstbase-tool.o -o cstbase-tool$(EXE) 


lib: $(OBJS)
	$(CC) $(LIBFLAGS) $(CFLAGS) $(OBJS) $(LIBS)


package: 
	@echo "Zipping up cstbase-tool for '$(PKGOS)'"
	zip cstbase-tool-$(PKGOS).zip cstbase-tool$(EXE)
	mkdir -f builds && cp cstbase-tool-$(PKGOKS).zip builds

clean: 
	rm -f $(OBJS)
	rm -f $(LIBTARGET)
	rm -f cstbase-tool.o hiddata.o

distclean: clean
	rm -f cstbase-tool$(EXE)
	rm -f $(LIBTARGET) $(LIBTARGET).a

# show shared library use
# in general we want minimal to no dependecies for cstbase-tool

# shows shared lib usage on Mac OS X
otool:
	otool -L cstbase-tool
# show shared lib usage on Linux
ldd:
	ldd cstbase-tool
# show shared lib usage on Windows
# FIXME: only works inside command prompt from
# Start->All Programs-> MS Visual Studio 2012 -> VS Tools -> Devel. Cmd Prompt
dumpbin: 
	dumpbin.exe /exports $(LIBTARGET)
	dumpbin.exe /exports cstbase-tool.exe


printvars:
	@echo "OS=$(OS), CFLAGS=$(CFLAGS), LDFLAGS=$(LDFLAGS), LIBS=$(LIBS), LIBFLAGS=$(LIBFLAGS)"
