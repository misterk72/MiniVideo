project(mini_extractor)
cmake_minimum_required(VERSION 2.6)


# Search for the custom "MiniVideo" module
SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})

# FindMiniVideo.cmake
FIND_PACKAGE(MiniVideo REQUIRED)

IF(MINIVIDEO_FOUND)
    MESSAGE(STATUS "* MiniVideo found")
ELSE(MINIVIDEO_FOUND)
    MESSAGE(ERROR " > MiniVideo not found!")
ENDIF(MINIVIDEO_FOUND)


# FindDoxygen.cmake
FIND_PACKAGE(Doxygen)

IF(DOXYGEN_FOUND)
    MESSAGE(STATUS "* Doxygen found")
ELSE(DOXYGEN_FOUND)
    MESSAGE(WARNING " > Doxygen not found!")
ENDIF(DOXYGEN_FOUND)


# Use "release" and "-O2" for maximum speed.
# Use "debug" if you want better support for GDB and Valgrind
MESSAGE(STATUS "* Building in DEBUG mode. You can turn off debug mode by editing 'CMakeLists.txt'.")
SET(CMAKE_BUILD_TYPE debug)
SET(CMAKE_C_FLAGS "-O0")


# Source files
SET(mini_extractor_SRC src/main.cpp src/main.h)


# Build the extractor software
ADD_EXECUTABLE(mini_extractor ${mini_extractor_SRC})
INCLUDE_DIRECTORIES(${MINIVIDEO_INCLUDE_DIR})


## Link against STATIC library:
## Please set FIND_PACKAGE manually
#FIND_PACKAGE(JPEG) # FindJPEG.cmake
#FIND_PACKAGE(PNG) # FindPNG.cmake
#ADD_LIBRARY(MiniVideo STATIC IMPORTED)
#SET_PROPERTY(TARGET MiniVideo PROPERTY IMPORTED_LOCATION ../../lib/build/libminivideo.a)
#TARGET_LINK_LIBRARIES(mini_extractor MiniVideo ${JPEG_LIBRARY} ${PNG_LIBRARY})


# Link against SHARED library:
LINK_DIRECTORIES(${MINIVIDEO_LIBRARY})
TARGET_LINK_LIBRARIES(mini_extractor ${MINIVIDEO_LIBRARY})


# Instal the extractor software into the system (optional step, requires root credentials)
INSTALL(TARGETS mini_extractor DESTINATION /usr/bin)
