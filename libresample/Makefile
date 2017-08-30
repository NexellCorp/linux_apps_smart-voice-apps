CROSS_COMPILE := arm-linux-gnueabihf-

DESTDIR     := ../lib
PKG_TYPE    := hf
PKG_CFLAGS  :=
PKG_LDFLAGS :=

ifeq ($(CC),cc)
CC  := $(CROSS_COMPILE)gcc
endif
ifeq ($(AR),ar)
AR  := $(CROSS_COMPILE)ar
endif
CXX := $(CROSS_COMPILE)g++

CFLAGS  := $(PKG_CFLAGS)
LDFLAGS := $(PKG_LDFLAGS)

CFLAGS  += -Wall
ifeq ($(PKG_TYPE),hf)
CFLAGS  += -mfloat-abi=hard
endif
ARFLAGS	:= crv

CFLAGS  += -O3
CFLAGS  += -fomit-frame-pointer

CXXFLAGS += $(CFLAGS)
CXXFLAGS += -std=c++11

COBJS	:= resample.o resample2.o
LIBS	+= -lm
OBJS	:= $(COBJS)

INCFILES := resample.h
TARGET   := libresample

all : $(TARGET)

$(TARGET) : depend $(OBJS)
	$(AR) $(ARFLAGS) $(TARGET).a $(OBJS)
	cp $(TARGET).a $(DESTDIR)/$(PKG_TYPE)/
	cp $(INCFILES) $(DESTDIR)

.PHONY: clean

clean :
	rm -rf $(OBJS) $(TARGET) core .depend *.a

ifeq (.depend,$(wildcard .depend))
include .depend
endif

depend dep:
	$(CC)  -M $(CFLAGS) $(COBJS:.o=.c) > .depend

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c -o $@ $<
