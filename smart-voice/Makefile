#
# Make options
#
#	CROSS_COMPILE	: define cross compiler (default arm-linux-gnueabihf-gcc)
#	DEBUG=y		: use debug build
#	DESTDIR=<path>	: set library path
#	PKG_CFLAGS="--sysroot=/home/rohan/temp/temp/yocto/sdk_yocto/tmp/sysroots/beaglebone"
#	PKG_LDFLAGS="--sysroot=/home/rohan/temp/temp/yocto/sdk_yocto/tmp/sysroots/beaglebone"
#

CROSS_COMPILE := arm-linux-gnueabihf-
PREPROCESS := n

DESTDIR     := ../lib
PKG_TYPE    := hf
PKG_CFLAGS  :=
PKG_LDFLAGS :=

ifeq ($(CC),cc)
CC  := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
endif
STRIP  ?= $(CROSS_COMPILE)strip

CFLAGS  := $(PKG_CFLAGS)
LDFLAGS := $(PKG_LDFLAGS)

CFLAGS  += -Wall
ifeq ($(PKG_TYPE),hf)
CFLAGS  += -mfloat-abi=hard
LDFLAGS += -mfloat-abi=hard
endif

CFLAGS  += -I $(DESTDIR)
CFLAGS  += -I $(DESTDIR)/$(PKG_TYPE)
ifeq ($(DEBUG),y)
CFLAGS  += -DDEBUG
CFLAGS  += -g
#CFLAGS  += -fno-omit-frame-pointer
#CFLAGS  += -fstack-protector-all

LDFLAGS += -rdynamic # '-rdynamic' or '-Wl,export-dynamic'
#LDFLAGS += -funwind-tables
#LDFLAGS += -static
else
CFLAGS  += -O2
CFLAGS  += -fomit-frame-pointer
LDFLAGS += -static
endif

LIBS	+= $(DESTDIR)/$(PKG_TYPE)/libtinyalsa.a
LIBS	+= $(DESTDIR)/$(PKG_TYPE)/libagcpdm.a
LIBS	+= $(DESTDIR)/$(PKG_TYPE)/libresample.a
LIBS	+= -lpthread
LIBS	+= -lm

ifeq ($(PREPROCESS),y)
LIBS	+= $(DESTDIR)/$(PKG_TYPE)/libPreproc1.a
CFLAGS  += -DSUPPORT_PRE_PROCESS
endif

CXXFLAGS += $(CFLAGS)
CXXFLAGS += -std=c++11

COBJS	:= util.o
CPPOBJS	:= smart_voice.o audioplay.o audiostream.o
OBJS	:= $(COBJS) $(CPPOBJS)

TARGET = smart_voice

all : $(TARGET)

$(TARGET) : depend $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
ifneq ($(DEBUG),y)
	$(STRIP) $@
endif

.PHONY: clean

clean :
	rm -rf $(OBJS) $(TARGET) core .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif

depend dep:
	$(CC)  -M $(CFLAGS) $(COBJS:.o=.c) > .depend
	$(CXX) -M $(CXXFLAGS) $(CPPOBJS:.o=.cpp) > .depend

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c -o $@ $<
