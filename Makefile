TARGET = ADShare

OBJS = \
       src/main.o \
       src/app.o \
       src/state.o \
       src/utils.o \
       src/hardware.o \
       src/ui.o \
       src/adhoc.o \
       src/data_protocol.o \
       src/transfer.o \
       src/browser.o \
       src/screens.o

INCDIR = src

CFLAGS = -O2 -G0 -Wall -Wextra
CXXFLAGS = -std=gnu++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics
ASFLAGS = $(CFLAGS)

LIBS = -losl -lintrafont -lpng -ljpeg -lz \
       -lpspkubridge -lpspexploit \
       -lpspnet_adhoc -lpspnet_adhocctl -lpspnet \
       -lpspwlan -lpsputility -lpsppower -lpsprtc \
       -lpspgum -lpspgu -lpsphprm -lpspumd \
       -lpspaudiolib -lpspaudio -lstdc++ -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = ADShare

# Optional: if ICON0.PNG exists in the project root, it is embedded in EBOOT.PBP.
ifneq ($(wildcard ICON0.PNG),)
PSP_EBOOT_ICON = ICON0.PNG
endif

PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
