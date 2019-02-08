
cmake_minimum_required(VERSION 2.8)

project(samples)

set(CMAKE_C_COMPILER gcc)

add_compile_options(-g -Wall)

set(BACKUP_BIN "cubrid_backup")
set(RESTORE_BIN "cubrid_restore")

set(BACKUP_HEADER "cubrid_backup_api.h")
set(BACKUP_LIB "libcubridbackup.so")

add_custom_command(
    OUTPUT ${BACKUP_HEADER}
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/../src/include/backup_api.h ${CMAKE_SOURCE_DIR}/cubrid_backup_api.h
    DEPENDS ${CMAKE_SOURCE_DIR}/../src/include/backup_api.h)

add_custom_target(
    link-backup-header ALL
    DEPENDS ${BACKUP_HEADER})

add_custom_command(
    OUTPUT ${BACKUP_LIB}
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/../${BACKUP_LIB}.1.0 ${CMAKE_SOURCE_DIR}/${BACKUP_LIB}
    DEPENDS ${CMAKE_SOURCE_DIR}/../${BACKUP_LIB}.1.0)

add_custom_target(
    link-backup-lib ALL
    DEPENDS ${BACKUP_LIB})

link_directories(${CMAKE_SOURCE_DIR})
link_directories(${CMAKE_SOURCE_DIR}/..)

add_executable(${BACKUP_BIN} ${BACKUP_BIN}.c)
target_link_libraries(${BACKUP_BIN} ${BACKUP_LIB} pthread)
add_dependencies(${BACKUP_BIN} link-backup-header link-backup-lib)

add_executable(${RESTORE_BIN} ${RESTORE_BIN}.c)
target_link_libraries(${RESTORE_BIN} ${BACKUP_LIB} pthread)
add_dependencies(${RESTORE_BIN} link-backup-header link-backup-lib)
