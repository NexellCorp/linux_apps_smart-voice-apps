CROSS_COMPILE := arm-linux-gnueabihf-

DESTDIR     := ../lib
PKG_TYPE    := hf
PKG_CFLAGS  :=
PKG_LDFLAGS :=

CFLAGS  := $(PKG_CFLAGS) 
LDFLAGS := $(PKG_LDFLAGS) 
CFLAGS  += -static

SRCDIR  = tinyalsa
TARGET  = libtinyalsa.a 

PHONY := all clean
all: src 

PHONY += src
src:
	make CROSS_COMPILE=$(CROSS_COMPILE) \
	CLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" \
	-C $(SRCDIR) || exit $?;
	cp $(SRCDIR)/src/$(TARGET) $(DESTDIR)/$(PKG_TYPE)/

clean:
	make -C $(SRCDIR) clean || exit $?; \

.PHONY: $(PHONY)
