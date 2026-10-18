#---------------------------------------------------------------------------------
# Kid Pix for Wii
#   ./scripts/build_controller_portlibs.sh   (once: libwiidrc / libwupc / libsicksaxis)
#   ./scripts/build_grrlib.sh                (once: GRRLIB)
#   make            -> boot.dol
#   make dist       -> dist/apps/kidpix/ ready to copy to the SD card
#---------------------------------------------------------------------------------
.SUFFIXES:
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPPC)/wii_rules

TARGET		:=	boot
BUILD		:=	build
SOURCES		:=	src libinput-wii/source
DATA		:=
INCLUDES	:=	src

CFLAGS	= -g -O2 -Wall -Wno-unused-function $(MACHDEP) $(INCLUDE) \
          -DHAVE_WIIDRC -DHAVE_WUPC -DHAVE_SICKSAXIS
CXXFLAGS	=	$(CFLAGS)
LDFLAGS	=	-g $(MACHDEP) -Wl,-Map,$(notdir $@).map -Wl,-wrap,wiiuse_accept

LIBS	:=	-lgrrlib -lwiidrc -lwupc -lsicksaxis -lpng -ljpeg -lfreetype -lbrotlidec -lbrotlicommon -lbz2 -lz \
            -lfat -lwiikeyboard -lwiiuse -lbte -lasnd -logc -lm

LIBDIRS	:=	$(CURDIR)/portlibs/ppc $(DEVKITPRO)/portlibs/ppc $(DEVKITPRO)/portlibs/wii $(CURDIR)/libinput-wii

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
export LD	:=	$(CC)
export OFILES	:=	$(CFILES:.c=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES), -iquote $(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD) -I$(LIBOGC_INC)
export LIBPATHS	:=	-L$(LIBOGC_LIB) $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean dist run

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol dist

dist: $(BUILD)
	@rm -rf dist && mkdir -p dist/apps/kidpix
	@cp boot.dol meta.xml dist/apps/kidpix/
	@[ ! -f icon.png ] || cp icon.png dist/apps/kidpix/
	@cp -R assets dist/apps/kidpix/assets
	@mkdir -p dist/apps/kidpix/saves
	@echo "dist/apps/kidpix ready - copy the 'apps' folder to the root of your SD card"

run:
	wiiload $(TARGET).dol

else

DEPENDS	:=	$(OFILES:.o=.d)
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)

endif
