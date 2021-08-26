TARGET      = psp-tests
OBJS        = src/main.o
    
INCDIR      = 
CFLAGS      = -G0 -Wall
CXXFLAGS    = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS     = $(CFLAGS)
BUILD_PRX   = 1
    
LIBDIR      =
LDFLAGS     =
LIBS        = -lpspgum -lpspgu -lm

EXTRA_TARGETS    = EBOOT.PBP
PSP_EBOOT_TITLE  = GOD HAND
PSP_EBOOT_ICON   = ICON0.PNG
PSP_EBOOT_PIC1   = PIC1.PNG
PSP_EBOOT_SND0   = SND0.AT3

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

run_sw:
	PPSSPPWindows64.exe -d -v --windowed "$(TARGET).prx"
	
run_hw:
	pspsh -e reset
	pspsh -e ./$(TARGET).prx