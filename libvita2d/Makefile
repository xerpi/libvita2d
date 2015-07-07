TARGET_LIB = libvita2d.a
OBJS       = source/vita2d.o source/vita2d_texture.o source/vita2d_draw.o source/utils.o
INCLUDES   = include
SHADERS    = shader/compiled/clear_v_gxp.o shader/compiled/clear_f_gxp.o \
             shader/compiled/color_v_gxp.o shader/compiled/color_f_gxp.o \
             shader/compiled/texture_v_gxp.o shader/compiled/texture_f_gxp.o

PREFIX  = arm-none-eabi
CC      = $(PREFIX)-gcc
AR      = $(PREFIX)-ar
CFLAGS  = -Wall -specs=psp2.specs -I$(INCLUDES)
ASFLAGS = $(CFLAGS)

all: $(TARGET_LIB)

debug: CFLAGS += -DDEBUG_BUILD
debug: all

$(TARGET_LIB): $(SHADERS) $(OBJS)
	$(AR) -rc $@ $^

#%.o: %.gxp
#	bin2s $^ > $(^:.gxp=.s)
#	$(CC) $(CFLAGS) -c $(^:.gxp=.s) -o $@

#%.frag.o: %.frag.cg
#	cgc -profile ps_3_0 $^ -o $(@:.o=.gxp)
#	bin2s $(@:.o=.gxp) > $(@:.o=.s)
#	$(CC) $(CFLAGS) -c $(@:.o=.s) -o $@

#%.vert.o: %.vert.cg
#	cgc -profile vs_3_0 $^ -o $(@:.o=.gxp)
#	bin2s $(@:.o=.gxp) > $(@:.o=.s)
#	$(CC) $(CFLAGS) -c $(@:.o=.s) -o $@

clean:
	@rm -rf $(TARGET_LIB) $(OBJS)

install: $(TARGET_LIB)
	@cp $(TARGET_LIB) $(PSP2SDK)/lib
	@cp include/vita2d.h $(PSP2SDK)/include
	@echo "Installed!"
