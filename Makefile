TARGET = EBOOT

OBJS = main.o

CFLAGS = -O2 -G0 -Wall -std=gnu99

LIBS = -lpspgum -lpspgu -lpspaudio -lpspaudiolib -lpsppower -lpspctrl -lpspdisplay -lpspge -lm

BUILD_PRX = 1
PSP_FW_VERSION = 660

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Ashfall
PSP_EBOOT_ICON = ICON0.PNG
PSP_EBOOT_PIC1 = PIC1.PNG

# Find the PSPSDK installation inside the PSPDEV environment.
PSPSDK := $(shell psp-config --pspsdk-path)

# PSPSDK build system
include $(PSPSDK)/lib/build.mak

clean:
	rm -f $(OBJS)
	rm -f EBOOT.PBP
	rm -f EBOOT.prx
	rm -f EBOOT.elf
	rm -f PARAM.SFO
