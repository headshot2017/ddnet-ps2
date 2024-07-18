SOURCE_DIRS	 := \
	src/base \
	src/engine/shared \
	src/engine/client \
	src/engine/external/json-parser \
	src/engine/external/md5 \
	src/engine/external/pnglite \
	src/engine/external/wavpack \
	src/game \
	src/game/client \
	src/game/client/components \
	src/game/editor \
	src/game/generated

BUILD_DIR     = build
BUILD_DIRS    = $(addprefix $(BUILD_DIR)/, $(SOURCE_DIRS))

CPP_FILES  := $(foreach dir,$(SOURCE_DIRS),$(wildcard $(dir)/*.cpp))
C_FILES    := $(foreach dir,$(SOURCE_DIRS),$(wildcard $(dir)/*.c))
OBJS       := $(addprefix $(BUILD_DIR)/, $(C_FILES:%.c=%.o) $(CPP_FILES:%.cpp=%.o))
EE_BIN 	    = ddnet-ps2.elf

# Dependency tracking
DEPFLAGS = -MT $@ -MMD -MP -MF $(BUILD_DIR)/$*.d
DEPFILES := $(OBJS:%.o=%.d)

EE_INCS 	:= -Isrc -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include -I$(PS2SDK)/ports/include/opus -I$(PS2SDK)/ports/include/freetype2
EE_CFLAGS   := -D_EE -G0 -O2 -Wall -gdwarf-2 -gz
EE_LDFLAGS  := -L$(PS2SDK)/ports/lib -L$(GSKIT)/lib -L$(PS2SDK)/ee/lib
EE_LIBS     := -lz -lpng -lcurl -lwolfssl -lopusfile -lopus -logg -lps2ip -lnetman -laudsrv -lpad -lgskit -ldmakit -lpacket -ldma -lgraph -ldraw -lmc
EE_LINKFILE := $(PS2SDK)/ee/startup/linkfile
EE_OBJS     := $(OBJS)


EE_IRX_FILES=\
	ps2dev9.irx \
	netman.irx \
	smap.irx \
	usbd.irx \
	bdm.irx \
	bdmfs_fatfs.irx \
	usbmass_bd.irx \
	freesd.irx \
	audsrv.irx

EE_IRX_SRCS = $(addprefix $(BUILD_DIR)/, $(addsuffix _irx.c, $(basename $(EE_IRX_FILES))))
EE_IRX_OBJS = $(addprefix $(BUILD_DIR)/, $(addsuffix _irx.o, $(basename $(EE_IRX_FILES))))
EE_OBJS += $(EE_IRX_OBJS)

# Where to find the IRX files
vpath %.irx $(PS2SDK)/iop/irx/

# Rule to generate them
$(BUILD_DIR)/%_irx.o: %.irx
	bin2c $< $(BUILD_DIR)/$*_irx.c $*_irx
	$(EE_CC) -c $(BUILD_DIR)/$*_irx.c -o $(BUILD_DIR)/$*_irx.o

# This is for the sbv patch
SBVLITE = $(PS2SDK)/sbv
EE_INCS += -I$(SBVLITE)/include
EE_LDFLAGS += -L$(SBVLITE)/lib
EE_LIBS += -lpatches





all: $(BUILD_DIRS) $(EE_BIN)

clean:
	rm -f $(EE_OBJS) $(EE_BIN) $(EE_IRX_SRCS)

run:
	ps2client execee host:$(EE_BIN)

reset:
	ps2client reset

include $(PS2SDK)/samples/Makefile.pref

# ELF generation
$(EE_BIN): src/game/generated $(EE_OBJS)
	$(EE_CXX) -T$(EE_LINKFILE) -O2 -o $(EE_BIN) $(EE_OBJS) $(EE_LDFLAGS) $(EE_LIBS)

# Object generation
$(BUILD_DIR)/%.o: %.c
	$(EE_CC) $(DEPFLAGS) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@
$(BUILD_DIR)/%.o: %.cpp
	$(EE_CXX) $(DEPFLAGS) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(BUILD_DIRS):
	mkdir -p $@

src/game/generated:
	@[ -d $@ ] || mkdir -p $@
	python datasrc/compile.py network_source > $@/protocol.cpp
	python datasrc/compile.py network_header > $@/protocol.h
	python datasrc/compile.py client_content_source > $@/client_data.cpp
	python datasrc/compile.py client_content_header > $@/client_data.h
	python scripts/cmd5.py src/engine/shared/protocol.h $@/protocol.h src/game/tuning.h src/game/gamecore.cpp $@/protocol.h > $@/nethash.cpp


# Dependency tracking
$(DEPFILES):

include $(wildcard $(DEPFILES))
