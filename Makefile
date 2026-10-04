TARGET = JAYJAYQUEST
OBJS = main.o
CFLAGS = -O2 -G0 -Wall
LIBS = -lpspgum -lpspgu -lpspdisplay -lpspge -lpspctrl -lm
BUILD_PRX = 1
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = JayJay Quest
PSP_EBOOT_ICON = ICON0.PNG
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

