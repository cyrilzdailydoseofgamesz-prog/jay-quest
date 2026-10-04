TARGET = EBOOT

OBJS = main.o

# PSP compiler settings
CFLAGS = -O2 -G0 -Wall -std=gnu99
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)

# Libraries used by the Ashfall PSP build.
# Add/remove libraries here if your main.c needs different PSP SDK modules.
LIBS = -lpspgum -lpspgu -lpspaudio -lpspaudiolib -lpsppower -lpspctrl -lpspdisplay -lpspge -lm

BUILD_PRX = 1
PSP_FW_VERSION = 660

# EBOOT.PBP metadata
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Ashfall
PSP_EBOOT_ICON = ICON0.PNG
PSP_EBOOT_PIC1 = PIC1.PNG

include $(PSPSDK)/lib/build.mak

# Generate the art/assets before building.
# Run manually with:
#   python3 tools/gen_assets.py
assets:
	python3 tools/gen_assets.py

# Clean compiled files.
clean:
	rm -f $(OBJS) EBOOT.PBP EBOOT ELF
	rm -rf build
