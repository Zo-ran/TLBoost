#
# Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.7.2)

project(sel4test-driver C)

set(configure_string "")

config_option(Sel4testHaveTimer HAVE_TIMER "Enable tests that require a timer driver" DEFAULT ON)

config_option(Sel4testSimulation SIMULATION "Disable tests not suitable for simulation" DEFAULT OFF)

config_option(
    Sel4testHaveCache
    HAVE_CACHE
    "Enable tests that require a functioning cache"
    DEFAULT
    ON
)
if(Sel4testAllowSettingsOverride)
    mark_as_advanced(CLEAR Sel4testHaveTimer Sel4testHaveCache)
else()
    mark_as_advanced(FORCE Sel4testHaveTimer Sel4testHaveCache)
endif()
add_config_library(sel4test-driver "${configure_string}")

find_package(musllibc REQUIRED)
find_package(util_libs REQUIRED)
find_package(seL4_libs REQUIRED)
find_package(sel4_projects_libs REQUIRED)

# Need to disable GC sections as it causes our tests to be stripped sometimes
# This option is tested in the following musllibc_setup_build_environment_with_sel4runtime call.
set(UserLinkerGCSections OFF CACHE BOOL "" FORCE)
# This sets up environment build flags and imports musllibc and runtime libraries.
musllibc_setup_build_environment_with_sel4runtime()
sel4_import_libsel4()
util_libs_import_libraries()
sel4_libs_import_libraries()
# Setting LibNanopb On causes its libraries to be created when sel4_projects_libs_import_libraries
# is called.
set(LibNanopb ON CACHE BOOL "" FORCE)
sel4_projects_libs_import_libraries()
add_subdirectory(../../libsel4testsupport libsel4testsupport)

file(
    GLOB
        static
        src/*.c
        src/tests/*.c
        src/arch/${KernelArch}/*.c
        src/plat/${KernelPlatform}/*.c
        src/arch/${KernelArch}/*.S
        src/plat/${KernelPlatform}/*.S
)

# Import build rules for test app
add_subdirectory(../sel4test-tests sel4test-tests)

add_executable(app app.c)
target_link_libraries(app sel4runtime sel4 muslc utils
                      sel4muslcsys sel4platsupport sel4utils sel4debug)

include(cpio)
MakeCPIO(archive.o "$<TARGET_FILE:app>")

# include(cpio)
# MakeCPIO(archive.o "$<TARGET_FILE:sel4test-tests>")

add_executable(sel4test-driver EXCLUDE_FROM_ALL ${static} archive.o)
target_include_directories(sel4test-driver PRIVATE "include")
target_link_libraries(
    sel4test-driver
    PUBLIC
        sel4_autoconf
        muslc
        sel4
        sel4runtime
        sel4allocman
        sel4vka
        sel4utils
        sel4rpc
        sel4test
        sel4platsupport
        sel4muslcsys
        sel4testsupport
    PRIVATE sel4test-driver_Config
)
target_compile_options(sel4test-driver PRIVATE -Werror -g)

# Set this image as the rootserver
include(rootserver)
DeclareRootserver(sel4test-driver)

# if(${sel4_arch} STREQUAL "loongarch64")
# 	set_target_properties(sel4test-driver PROPERTIES  LINK_FLAGS " -z max-page-size=16384 ")
# endif()

# if(${sel4_arch} STREQUAL "loongarch64")
#     set(linker_lds_path "${CMAKE_CURRENT_SOURCE_DIR}/sel4test-driver_loongarch.lds")
#     set_target_properties(sel4test-driver PROPERTIES  LINK_FLAGS "-Wl,-T ${linker_lds_path} ")
# endif()

# if(${sel4_arch} STREQUAL "loongarch64")
#     set_target_properties(sel4test-driver PROPERTIES  LINK_FLAGS " -Wall -Wl,--section-start=.init=0x10000 ")
# endif()