TARGET = HollowBay
OBJS = main.o
CFLAGS = -O2 -G0 -Wall -std=gnu99
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
LIBS = -lpspgum -lpspgu -lpspvfpu -lpspdebug -lpspdisplay -lpspge -lm
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Echoes of Hollow Bay
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
