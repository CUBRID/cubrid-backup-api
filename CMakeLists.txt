
cmake_minimum_required(VERSION 2.8)

project(backup-api)

set(BACKUP_API_MAJOR_VERSION "1")
set(BACKUP_API_MINOR_VERSION "0")

set(BACKUP_API_SOURCES
    ${CMAKE_SOURCE_DIR}/src/backup_api.c
    ${CMAKE_SOURCE_DIR}/src/backup_core.c
    ${CMAKE_SOURCE_DIR}/src/backup_manager.c
    ${CMAKE_SOURCE_DIR}/src/handle_manager.c)

include_directories(src/include)

set(CMAKE_C_COMPILER "gcc")

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    #add_compile_options(-g -Wall)
else()
    message(STATUS "No build type selected, default to Release")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (default Release)")
    #add_compile_options(-g -Wall)
endif()

set(BACKUP_LIB "cubridbackup")

add_library(${BACKUP_LIB} SHARED ${BACKUP_API_SOURCES})
set_target_properties(${BACKUP_LIB} PROPERTIES SOVERSION "${BACKUP_API_MAJOR_VERSION}.${BACKUP_API_MINOR_VERSION}")

# TODO
# 1) debug / release 구분
# 2) 빌드 디렉토리 구분
#    라이브러리 및 헤더파일 생성 디렉토리 분리 ==> build_xxx_xx_debug
#      CMAKE_BINARY_DIR = /home/hornetmj/work/CUBRID_10/build_x86_64_debug
# 3) tar
