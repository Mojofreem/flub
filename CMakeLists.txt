cmake_minimum_required (VERSION 2.8.0)
project (flub)
SET(FLUB_VERSION_MAJOR 0)
SET(FLUB_VERSION_MINOR 1)

# https://github.com/tcbrindle/sdl2-cmake-scripts
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")
FIND_PACKAGE(SDL2 REQUIRED)
FIND_PACKAGE(SDL2_mixer REQUIRED)
FIND_PACKAGE(OpenGL REQUIRED)
IF(MINGW)
    SET(ZLIB_ROOT "C:/Mingw/")
ENDIF(MINGW)
FIND_PACKAGE(ZLIB REQUIRED)
INCLUDE_DIRECTORIES(${SDL2_INCLUDE_DIR}
                    ${SDL2_MIXER_INCLUDE_DIR}
                    ${OPENGL_INCLUDE_DIR})

SET(CMAKE_C_FLAGS "-std=gnu99 -DJSMN_PARENT_LINKS=1" CACHE STRING "" FORCE)
SET(CMAKE_COLOR_MAKEFILE ON)

SET(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
#SET(DOXYGEN_FOR_ALL ALL)

SET(FLUB_DEBUG 1)

IF (FLUB_DEBUG)
    MESSAGE (STATUS "Debug mode")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFLUB_DEBUG=1")
ELSE (FLUB_DEBUG)
    MESSAGE (STATUS "Release mode")
ENDIF (FLUB_DEBUG)


if (WIN32)
    SET(MINGW mingw32)
    SET(WINDOWS_GLEW "-DGLEW_NO_GLU")
    SET(SDL2_INCLUDE_DIRS "C:/MinGW/include/SDL2")
    SET(SDL2_LIBRARIES SDL2main SDL2)
    SET(OPENGL_LIBRARIES opengl32 glu32 glaux)
    SET(PHYSFS_LIBRARIES physfs)
    SET(DOXYGEN_FOUND 1)
    SET(DOXYGEN_EXECUTABLE doxygen)
    SET(ZLIB_LIBRARY libz)
else (WIN32)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        MESSAGE(STATUS "Build is Mac OSX")
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMACOSX=1")
        FIND_PACKAGE(SDL2 REQUIRED)
        INCLUDE_DIRECTORIES(${SDL2_INCLUDE_DIR})
        #LIST(APPEND SDL2_INCLUDE_DIRS "/Library/Frameworks/SDL2.framework/Headers")
        #LIST(APPEND SDL2_INCLUDE_DIRS "/Library/Frameworks/SDL2_mixer.framework/Headers")
        #LINK_DIRECTORIES ("/Library/Frameworks/SDL2.framework")
        #LINK_DIRECTORIES ("/Library/Frameworks/SDL2_mixer.framework")
        SET(SDL2_LIBRARIES SDL2)
        #SET(OPENGL_LIBRARIES opengl32 glu32 glaux)
        #SET(OPENGL_LIBRARIES opengl glu glaux)
        SET(PHYSFS_LIBRARIES physfs)

        FIND_LIBRARY(IOKIT_FRAMEWORK IOKit)
        MESSAGE("Finding library IOKit: ${IOKIT_FRAMEWORK}")
    else (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	    INCLUDE (FindPkgConfig)
	    PKG_SEARCH_MODULE (SDL2 REQUIRED sdl2)
	    PKG_SEARCH_MODULE (OPENGL REQUIRED gl)
        FIND_PACKAGE(Doxygen)
    endif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
endif (WIN32)

SET(SDL2_AUX_LIBRARIES SDL2_mixer)

INCLUDE_DIRECTORIES (${SDL2_INCLUDE_DIRS})
INCLUDE_DIRECTORIES (${OPENGL_INCLUDE_DIRS})

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR})
INCLUDE_DIRECTORIES ("3rdparty/physfs-2.0.3")
LINK_DIRECTORIES ("${PROJECT_SOURCE_DIR}/3rdparty/physfs-2.0.3")

###############################################################################
# stbfont tool takes an TTF font  file, and generates a compileable source file
# and/or a binary stb font file.
###############################################################################
#add_executable (stbfont tools/stbfont.c)
#TARGET_LINK_LIBRARIES (stbfont ${MINGW})

###############################################################################
# memfile tool takes a list of files, and generates a compileable source file
# that can be used to store resources within an executable or library.
###############################################################################
add_executable (memfile tools/memfile.c)
TARGET_LINK_LIBRARIES (memfile ${MINGW})

###############################################################################
# Flub library
###############################################################################
CONFIGURE_FILE("${PROJECT_SOURCE_DIR}/flub/flubconfig.h.in" "${PROJECT_BINARY_DIR}/flub/flubconfig.h")

LIST (APPEND FLUB_SRC
    source/anim.c
    source/app.c
    source/audio.c
    source/cmdline.c
    source/config.c
    source/console.c
    source/core.c
    source/data/critbit.c
    source/data/circbuf.c
    source/font.c
    source/gfx.c
    #source/gui.c
    source/input.c
    source/layout.c
    source/license.c
    source/logger.c
    source/memory.c
    source/module.c
    source/physfsrwops.c
    source/physfsutil.c
    source/sdl.c
    source/simpleedit.c
    source/texture.c
    source/theme.c
    source/util/mime.c
    source/util/misc.c
    source/util/enum.c
    source/util/color.c
    source/util/file.c
    source/util/jsmn.c
    source/util/parse.c
    source/util/string.c
    source/video.c
    #source/widget.c
    source/3rdparty/jsmn/jsmn.c
)

IF (WIN32)
    SET(FLUB_SRC ${FLUB_SRC} source/3rdparty/glew/glew.c)
ENDIF (WIN32)

###############################################################################
# Flub resources
###############################################################################

# Set the output directory for generated files, and create it.
SET(FLUB_RESOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/resources")
FILE(MAKE_DIRECTORY ${FLUB_RESOURCE_DIR})
ADD_CUSTOM_TARGET(build-resource-dir ALL COMMAND ${CMAKE_COMMAND} -E make_directory ${FLUB_RESOURCE_DIR})
MESSAGE(STATUS "Resource dir: [${FLUB_RESOURCE_DIR}]")

FUNCTION(FLUBRESOURCE FILE_PATH AS_PATH)
    SET(FLUB_RES_DEP_LIST ${FLUB_RES_DEP_LIST} DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/${FILE_PATH} PARENT_SCOPE)
    SET(FLUB_RES_INC_LIST ${FLUB_RES_INC_LIST} ${CMAKE_CURRENT_SOURCE_DIR}/assets/${FILE_PATH} -p resources/${AS_PATH} PARENT_SCOPE)
ENDFUNCTION(FLUBRESOURCE)

# Texture resources
FLUBRESOURCE(data/texlist.txt data/texlist.txt)
FLUBRESOURCE(images/flub-simple-gui.gif images/flub-simple-gui.gif)
FLUBRESOURCE(images/flub-keycap-misc.gif images/flub-keycap-misc.gif)
FLUBRESOURCE(images/flub-slice-test.gif images/flub-slice-test.gif)

# Sound resources
FLUBRESOURCE(sounds/click.wav sounds/click.wav)
FLUBRESOURCE(sounds/consoleclose.wav sounds/consoleclose.wav)
FLUBRESOURCE(sounds/consoleopen.wav sounds/consoleopen.wav)
FLUBRESOURCE(sounds/menumove.wav sounds/menumove.wav)
FLUBRESOURCE(sounds/menuselect.wav sounds/menuselect.wav)

# OpenGL shader resources
FLUBRESOURCE(shaders/simple.vert shaders/simple.vert)
FLUBRESOURCE(shaders/simple.frag shaders/simple.frag)
FLUBRESOURCE(shaders/stbfont.frag shaders/stbfont.frag)

###############################################################################
# Font conversion utility
###############################################################################

FUNCTION(FONT2BIN INL_PATH BASE_NAME FONT_NAME FONT_SIZE FONT_BOLD FONT_MONO BIN_NAME FLUB_INC)
    ADD_EXECUTABLE(stbfont_gen_${BASE_NAME} tools/font2bin.c)
    SET_TARGET_PROPERTIES(stbfont_gen_${BASE_NAME} PROPERTIES COMPILE_FLAGS "-DFONT2BIN_STB_INL_FILE=\"\\\"${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/${INL_PATH}\\\"\" -DFONT2BIN_BASE=${BASE_NAME} -DFONT2BIN_STB_BIN_FILE=\"${FLUB_RESOURCE_DIR}/${BIN_NAME}\" -DFONT2BIN_FONT_NAME=\"${FONT_NAME}\" -DFONT2BIN_FONT_SIZE=${FONT_SIZE} -DFONT2BIN_FONT_BOLD=${FONT_BOLD} -DFONT2BIN_FONT_MONO=${FONT_MONO}")
    ADD_CUSTOM_COMMAND(OUTPUT ${FLUB_RESOURCE_DIR}/${BIN_NAME} MAIN_DEPENDENCY ${CMAKE_SOURCE_DIR}/assets/fonts/${INL_PATH} DEPENDS stbfont_gen_${BASE_NAME} COMMAND stbfont_gen_${BASE_NAME} COMMENT "Generating ${FONT_NAME} ${FONT_SIZE} binary font file")
    IF (FLUB_INC)
        SET(FLUB_RES_DEP_LIST ${FLUB_RES_DEP_LIST} DEPENDS ${FLUB_RESOURCE_DIR}/${BIN_NAME} PARENT_SCOPE)
        SET(FLUB_RES_INC_LIST ${FLUB_RES_INC_LIST} ${FLUB_RESOURCE_DIR}/${BIN_NAME} -p resources/font/${BIN_NAME} PARENT_SCOPE)
    ENDIF (FLUB_INC)
ENDFUNCTION(FONT2BIN)

FONT2BIN("stb_font_consolas_12_usascii.inl" consolas_12_usascii consolas 12 0 0 "consolas.12.stbfont" 1)
FONT2BIN("stb_font_consolas_bold_12_usascii.inl" consolas_bold_12_usascii consolas 12 1 0 "consolas.12.bold.stbfont" 1)
FONT2BIN("stb_font_courier_12_usascii.inl" courier_12_usascii courier 12 0 1 "courier.12.stbfont" 1)
FONT2BIN("stb_font_courier_bold_12_usascii.inl" courier_bold_12_usascii courier 12 1 1 "courier.12.bold.stbfont" 1)
FONT2BIN("stb_font_times_12_usascii.inl" times_12_usascii times 12 0 0 "times.12.stbfont" 0)

###############################################################################
# Flub compiled resources
###############################################################################

# Generate the compiled file structure, and include it in the library.
#MESSAGE(STATUS "Deps: ${FLUB_RES_DEP_LIST}")
#MESSAGE(STATUS "Inc: ${FLUB_RES_INC_LIST}")
ADD_CUSTOM_COMMAND(OUTPUT ${FLUB_RESOURCE_DIR}/flub_resources.c ${FLUB_RESOURCE_DIR}/flub_resources.h DEPENDS memfile ${FLUB_RES_DEP_LIST} COMMAND memfile ${FLUB_RESOURCE_DIR}/flub_resources flubDefaultResources -b ${FLUB_RESOURCE_DIR}/ ${FLUB_RES_INC_LIST} COMMENT "Building Flub resource cache source file.")
LIST(APPEND FLUB_SRC ${FLUB_RESOURCE_DIR}/flub_resources.c)

###############################################################################
# Flub library
###############################################################################

ADD_LIBRARY (flub ${FLUB_SRC})

###############################################################################
# Generate the TODO list file for the flub library
###############################################################################

#ADD_CUSTOM_COMMAND(TARGET flub PRE_BUILD COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/todo.sh ${CMAKE_CURRENT_SOURCE_DIR}/flubtodo.txt "${CMAKE_CURRENT_SOURCE_DIR}/flub/\\*" "-d skip")

###############################################################################
# Doxygen
###############################################################################
# https://majewsky.wordpress.com/2010/08/14/tip-of-the-day-cmake-and-doxygen/

IF (DOXYGEN_FOUND)
CONFIGURE_FILE (${CMAKE_CURRENT_SOURCE_DIR}/doxygen/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/doxygen/Doxyfile @ONLY)
ADD_CUSTOM_TARGET (doc
    ${DOXYGEN_FOR_ALL}
    ${DOXYGEN_EXECUTABLE}
    ${CMAKE_CURRENT_BINARY_DIR}/doxygen/Doxyfile
    WORKING_DIRECTORY ${DOXYGEN_OUTPUT_DIR}
    COMMENT "Generating API documentation with Doxygen" VERBATIM
)
endif (DOXYGEN_FOUND)

###############################################################################
# Flub library test application
###############################################################################
FUNCTION(FLUB_APP_DEPS TARGET)
IF (WIN32)
    TARGET_LINK_LIBRARIES (${TARGET} mingw32)
ENDIF (WIN32)
TARGET_LINK_LIBRARIES (${TARGET} flub)
TARGET_LINK_LIBRARIES (${TARGET} physfs-flub)
IF (WIN32)
    #TARGET_LINK_LIBRARIES (${TARGET} SDL2main)
ENDIF (WIN32)
#TARGET_LINK_LIBRARIES (${TARGET} SDL2)
#TARGET_LINK_LIBRARIES (${TARGET} SDL2_mixer)
IF (WIN32)
    TARGET_LINK_LIBRARIES (${TARGET} opengl32)
    TARGET_LINK_LIBRARIES (${TARGET} glu32)
ELSE (WIN32)
    #TARGET_LINK_LIBRARIES (${TARGET} opengl)
    TARGET_LINK_LIBRARIES (${TARGET} ${OPENGL_LIBRARIES})
    target_link_libraries(${TARGET} ${IOKIT_FRAMEWORK})
    #TARGET_LINK_LIBRARIES (${TARGET} glu)
ENDIF (WIN32)    
#TARGET_LINK_LIBRARIES (${TARGET} glaux)
TARGET_LINK_LIBRARIES (${TARGET} ${MWINDOWS})
TARGET_LINK_LIBRARIES (${TARGET} ${WINDOWS_GLEW})
TARGET_LINK_LIBRARIES (${TARGET} ${SDL2_LIBRARY}
                                 ${SDL2_MIXER_LIBRARIES})
TARGET_LINK_LIBRARIES (${TARGET} ${ZLIB_LIBRARY})
ENDFUNCTION(FLUB_APP_DEPS)


ADD_EXECUTABLE (flubtest test/test.c)
FLUB_APP_DEPS (flubtest)

#ADD_EXECUTABLE (themer tools/themer.c)
#FLUB_APP_DEPS (themer)

#ADD_CUSTOM_COMMAND(OUTPUT ${FLUB_RESOURCE_DIR}/flub_calibrate.c ${FLUB_RESOURCE_DIR}/flub_calibrate.h DEPENDS memfile DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/work/flub-calibrate.gif COMMAND memfile ${FLUB_RESOURCE_DIR}/flub_calibrate flubResCalibrate -b ${FLUB_RESOURCE_DIR}/ ${CMAKE_CURRENT_SOURCE_DIR}/work/flub-calibrate.gif -p resources/images/flub-calibrate.gif COMMENT "Building pixperfect resource cache source file.")
#ADD_EXECUTABLE (pixperfect test/pixperfect.c ${FLUB_RESOURCE_DIR}/flub_calibrate.c)
#FLUB_APP_DEPS (pixperfect)

#ADD_EXECUTABLE(circbuftest test/circbuftest.c)
#FLUB_APP_DEPS(circbuftest)
