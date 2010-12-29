TARGET = mhsc
OBJS = main.o

USE_KERNEL_LIBC = 1
USE_KERNEL_LIBS = 1

INCDIR = 
CFLAGS = -O2 -G0 -Wall -fno-builtin-printf
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =

LIBS = -lpspumd

PSPSDK=$(shell psp-config --pspsdk-path)
EXTRA_CLEAN=clean_release
include $(PSPSDK)/lib/build_prx.mak

LIBS += -lpspge

release: all
	mkdir -p release/seplugins/mhsc
	cp $(TARGET).prx release/seplugins/mhsc
	echo "ms0:/seplugins/mhsc.prx 1" > release/seplugins/game.txt
	cp README release/readme.txt
	cp COPYING release/license.txt

clean_release:
	rm -rf release
