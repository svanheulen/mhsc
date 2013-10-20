TARGET = mhsc
OBJS = main.o

USE_KERNEL_LIBC = 1
USE_KERNEL_LIBS = 1

INCDIR =
CFLAGS = -O2 -G0 -Wall -fno-builtin-printf
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =

LIBS =

PSPSDK=$(shell psp-config --pspsdk-path)
EXTRA_CLEAN=clean_release
include $(PSPSDK)/lib/build_prx.mak

LIBS += -lpspge

release: all
	mkdir -p release/seplugins/mhsc
	cp $(TARGET).prx release/seplugins/mhsc
	echo "ms0:/seplugins/mhsc/mhsc.prx 1" > release/seplugins/game.txt
	sed -e "s/$$/\r/" < README > release/readme.txt
	sed -e "s/$$/\r/" < COPYING > release/license.txt

clean_release:
	rm -rf release
