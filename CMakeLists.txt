cmake_minimum_required(VERSION 3.20)
project(lwext4 C)


include_directories(include)
include_directories(${PROJECT_BINARY_DIR}/include)
include_directories(blockdev/filedev)
include_directories(blockdev/filedev_win)

set(BLOCKDEV_TYPE  none)

add_definitions(-DCONFIG_USE_DEFAULT_CONFIG=0)
add_definitions(-DVERSION="${VERSION}")

    add_definitions(-DCONFIG_DEBUG_PRINTF=0)
    add_definitions(-DCONFIG_DEBUG_ASSERT=0)
    add_definitions(-DCONFIG_HAVE_OWN_OFLAGS=1)
    add_definitions(-DCONFIG_HAVE_OWN_ERRNO=0)
    add_definitions(-DCONFIG_BLOCK_DEV_CACHE_SIZE=16)
    add_definitions(-DCONFIG_USE_DEFAULT_CFG=1)


# add_subdirectory(blockdev)

#Library build
add_subdirectory(src)
#Detect all possible warnings for lwext4 target
if (NOT CMAKE_COMPILER_IS_GNUCC)
    set_target_properties(lwext4 PROPERTIES COMPILE_FLAGS "")
else()
    set_target_properties(lwext4 PROPERTIES COMPILE_FLAGS "-Wall -Wextra -pedantic")
endif()

