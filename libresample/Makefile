#CROSS_COMPILE := arm-linux-gnueabihf-
CROSS_COMPILE := arm-poky-linux-gnueabi-
LIB_DIR := ../lib
INC_DIR := ../lib

CC  = $(CROSS_COMPILE)gcc
CPP = $(CROSS_COMPILE)g++
AR  = $(CROSS_COMPILE)ar

CFLAGS  := -Wall
CFLAGS  += -mfloat-abi=hard
ARFLAGS	:= crv

CFLAGS  += -O3
CFLAGS  += -fomit-frame-pointer

CFLAGS	+= -march=armv7-a -mfpu=neon -mcpu=cortex-a9 --sysroot=/opt/poky/2.1.2/sysroots/cortexa9hf-neon-poky-linux-gnueabi
CXXFLAGS+= -march=armv7-a -mfpu=neon -mcpu=cortex-a9 --sysroot=/opt/poky/2.1.2/sysroots/cortexa9hf-neon-poky-linux-gnueabi

COBJS	:= resample.o resample2.o
LIBS	+= -lm
OBJS	:= $(COBJS) $(CPPOBJS)

INC_FILES := resample.h

TARGET = libresample

all : $(TARGET)

$(TARGET) : depend $(OBJS)
	$(AR) $(ARFLAGS) $(TARGET).a $(OBJS)
	cp  $(TARGET).a $(LIB_DIR)
	cp $(INC_FILES) $(INC_DIR)

.PHONY: clean

clean :
	rm -rf $(OBJS) $(TARGET) core .depend *.a

ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp)
depend dep:
	$(CC)  -M $(CFLAGS) $(SRCS) > .depend

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

%.o: %.cpp
	$(CPP) $(CFLAGS) $(INCLUDE) -c -o $@ $<
