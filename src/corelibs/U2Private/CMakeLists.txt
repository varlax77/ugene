cmake_minimum_required(VERSION 3.4)
project(U2Private)

set(CMAKE_AUTOMOC ON)

if (APPLE)
    find_package(Qt5 REQUIRED Core Gui Widgets Xml)
else()
    find_package(Qt5 REQUIRED Core Gui Widgets)
endif()

add_definitions(-DBUILDING_U2PRIVATE_DLL)

include_directories(src)
include_directories(../../include)
include_directories(../../libs_3rdparty/breakpad/src)

file(GLOB_RECURSE SRCS src/*.cpp)
file(GLOB_RECURSE HDRS src/*.h)

file(GLOB_RECURSE WIN_IGNORE_FILES src/*Mac.* src/*UnixNotMac.*)
file(GLOB_RECURSE MAC_IGNORE_FILES src/*Win.* src/*UnixNotMac.*)
file(GLOB_RECURSE LINUX_IGNORE_FILES src/*[er]Mac.* src/*Win.*)

if (WIN32)
    list(REMOVE_ITEM SRCS ${WIN_IGNORE_FILES})
    list(REMOVE_ITEM HDRS ${WIN_IGNORE_FILES})
elseif (APPLE)
    list(REMOVE_ITEM SRCS ${MAC_IGNORE_FILES})
    list(REMOVE_ITEM HDRS ${MAC_IGNORE_FILES})
elseif (UNIX)
    list(REMOVE_ITEM SRCS ${LINUX_IGNORE_FILES})
    list(REMOVE_ITEM HDRS ${LINUX_IGNORE_FILES})
endif ()

add_library(U2Private SHARED ${HDRS} ${SRCS})

target_link_libraries(U2Private
        Qt5::Core Qt5::Gui Qt5::Widgets
        breakpad
        U2Core U2Algorithm U2Formats)

if (APPLE)
    target_link_libraries(U2Private  Qt5::Xml "-framework CoreFoundation -framework IOKit")
endif()
