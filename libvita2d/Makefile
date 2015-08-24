TARGET_LIB = libvita2d.a
OBJS       = source/vita2d.o source/vita2d_texture.o source/vita2d_draw.o source/utils.o \
             source/vita2d_image_png.o source/vita2d_image_jpeg.o source/vita2d_image_bmp.o \
             source/vita2d_font.o source/bin_packing_2d.o source/texture_atlas.o \
             source/int_htab.o
INCLUDES   = include
SHADERS    = shader/compiled/clear_v_gxp.o shader/compiled/clear_f_gxp.o \
             shader/compiled/color_v_gxp.o shader/compiled/color_f_gxp.o \
             shader/compiled/texture_v_gxp.o shader/compiled/texture_f_gxp.o \
             shader/compiled/texture_tint_f_gxp.o

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc
AR      = $(PREFIX)-ar
CFLAGS  = -Wl,-q -Wall -O3 -I$(INCLUDES) -I$(PSP2SDK)/include/freetype2
ASFLAGS = $(CFLAGS)

all: $(TARGET_LIB)

debug: CFLAGS += -DDEBUG_BUILD
debug: all

$(TARGET_LIB): $(SHADERS) $(OBJS)
	$(AR) -rc $@ $^

clean:
	@rm -rf $(TARGET_LIB) $(OBJS)

install: $(TARGET_LIB)
	@cp $(TARGET_LIB) $(VITASDK)/arm-vita-eabi/lib
	@cp include/vita2d.h $(VITASDK)/arm-vita-eabi/include
	@echo "Installed!"
