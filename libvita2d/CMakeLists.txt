cmake_minimum_required(VERSION 3.0)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VITASDK})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
    else()
        message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
    endif()
endif()


project(libvita2d)
include("${VITASDK}/share/vita.cmake" REQUIRED)

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

set(SHADERS_FILES
        shader/compiled/clear_v_gxp.cg
        shader/compiled/clear_f_gxp.cg
        shader/compiled/color_v_gxp.cg
        shader/compiled/color_f_gxp.cg
        shader/compiled/texture_v_gxp.cg
        shader/compiled/texture_f_gxp.cg
        shader/compiled/texture_tint_f_gxp.cg)

set(INC_FILES
        include)

set(TARGET_LIB
        ${SRC_FILES}
        ${SHADER_FILES})

set(CFLAGS -Wl,-q -Wall -O3 -I${INC_FILES} -I$ENV{VITASDK}/arm-vita-eabi/include/freetype2 -ffat-lto-objects -flto)

add_library(${PROJECT_NAME} STATIC ${TARGET_LIB})
target_include_directories(${PROJECT_NAME} PRIVATE ${INC_FILES})
target_compile_options(${PROJECT_NAME} PRIVATE ${CFLAGS})

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(FILES include/vita2d.h DESTINATION include)