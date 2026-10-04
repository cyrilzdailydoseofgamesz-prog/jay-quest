TARGET = JAYJAYQUEST
OBJS = main.o
CFLAGS = -O2 -G0 -Wall
LIBS = -lpspdisplay -lpspge -lpspctrl
BUILD_PRX = 1
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = JayJay Quest
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
