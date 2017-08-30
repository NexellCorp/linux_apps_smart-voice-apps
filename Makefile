CT ?= arm-linux-gnueabihf-
FP ?= hf

CROSS_COMPILE = $(CT)

PT ?= $(FP)
DESTDIR ?= $(shell pwd)/lib
PKG_CFLAGS :=
PKG_LDFLAGS :=

SRCDIR  = smart-voice

LIBDIR  = libresample

PHONY := all clean
all: lib src

PHONY += lib
lib:
	@for dir in $(LIBDIR); do \
		make DESTDIR=$(DESTDIR) PKG_TYPE=$(PT) DEBUG=$(DEBUG) \
                CROSS_COMPILE=$(CROSS_COMPILE) \
		PKG_CFLAGS="$(PKG_CFLAGS)" PKG_LDFLAGS="$(PKG_LDFLAGS)" \
		-C $$dir || exit $?; \
        done

PHONY += src
src:
	@for dir in $(SRCDIR); do \
                make DESTDIR=$(DESTDIR) PKG_TYPE=$(PT) DEBUG=$(DEBUG) \
                CROSS_COMPILE=$(CROSS_COMPILE) \
		PKG_CFLAGS="$(PKG_CFLAGS)" PKG_LDFLAGS="$(PKG_LDFLAGS)" \
		-C $$dir || exit $?; \
        done

clean:
	@for dir in $(SRCDIR); do \
                make -C $$dir clean || exit $?; \
        done
	@for dir in $(LIBDIR); do \
                make -C $$dir clean || exit $?; \
        done

help:                                                                           
	@echo 'help'
	@echo  '  make for make all - Build all directory'
	@echo  '  clean             - Remove most generated files'
	@echo  '  src               - Build application directory'
	@echo  '  lib               - Build library directory'
	@echo  ''
	@echo  '  make CT=arm-xxx- '
	@echo  '       Set CROSS_COMPILE, default $(CROSS_COMPILE)'
	@echo  '  make PT=sf|hf '
	@echo  '       Set pacakge type (hf/sf/android/..), default $(PKG_TYPE)'
	@echo  '  make DESTDIR='lib path''
	@echo  '       Set install directory, default $(DESTDIR)'
	@echo  '  make DEBUG=y '
	@echo  '       Build with debug mode, default no'

.PHONY: $(PHONY)
