cmake_minimum_required(VERSION 3.0)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VITASDK})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
    else()
        message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
    endif()
endif()

project(vita2d)
include("${VITASDK}/share/vita.cmake" REQUIRED)
include_directories(include)

set(SRC_FILES
        source/vita2d.c
        source/vita2d_texture.c
        source/vita2d_draw.c
        source/utils.c
        source/vita2d_image_png.c
        source/vita2d_image_jpeg.c
        source/vita2d_image_bmp.c
        source/vita2d_font.c
        source/vita2d_pgf.c
        source/vita2d_pvf.c
        source/bin_packing_2d.c
        source/texture_atlas.c
        source/int_htab.c)

set(SHADER_FILES
        shader/compiled/clear_v_gxp.o
        shader/compiled/clear_f_gxp.o
        shader/compiled/color_v_gxp.o
        shader/compiled/color_f_gxp.o
        shader/compiled/texture_v_gxp.o
        shader/compiled/texture_f_gxp.o
        shader/compiled/texture_tint_f_gxp.o
        )

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O3 -I$(VITASDK)/arm-vita-eabi/include/freetype2 -ffat-lto-objects -flto")

add_library(${PROJECT_NAME}
        ${SHADER_FILES}
        ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} freetype)

install(FILES ${PROJECT_BINARY_DIR}/lib${PROJECT_NAME}.a DESTINATION lib)
install(FILES ${PROJECT_SOURCE_DIR}/include/vita2d.h DESTINATION include)