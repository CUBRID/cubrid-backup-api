
cmake_minimum_required(VERSION 2.8)

project(samples)

set(CMAKE_C_COMPILER gcc)

add_compile_options(-g -Wall)

set(BACKUP_TEST01 "bk_test01")
set(BACKUP_TEST02 "bk_test02")
set(BACKUP_TEST03 "bk_test03")

set(RESTORE_TEST01 "rs_test01")
set(RESTORE_TEST02 "rs_test02")
set(RESTORE_TEST03 "rs_test03")

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

# backup
add_executable(${BACKUP_TEST01} ${BACKUP_TEST01}.c)
target_link_libraries(${BACKUP_TEST01} ${BACKUP_LIB} pthread)
add_dependencies(${BACKUP_TEST01} link-backup-header link-backup-lib)

add_executable(${BACKUP_TEST02} ${BACKUP_TEST02}.c)
target_link_libraries(${BACKUP_TEST02} ${BACKUP_LIB} pthread)
add_dependencies(${BACKUP_TEST02} link-backup-header link-backup-lib)

add_executable(${BACKUP_TEST03} ${BACKUP_TEST03}.c)
target_link_libraries(${BACKUP_TEST03} ${BACKUP_LIB} pthread)
add_dependencies(${BACKUP_TEST03} link-backup-header link-backup-lib)

# restore
add_executable(${RESTORE_TEST01} ${RESTORE_TEST01}.c)
target_link_libraries(${RESTORE_TEST01} ${BACKUP_LIB} pthread)
add_dependencies(${RESTORE_TEST01} link-backup-header link-backup-lib)

add_executable(${RESTORE_TEST02} ${RESTORE_TEST02}.c)
target_link_libraries(${RESTORE_TEST02} ${BACKUP_LIB} pthread)
add_dependencies(${RESTORE_TEST02} link-backup-header link-backup-lib)

add_executable(${RESTORE_TEST03} ${RESTORE_TEST03}.c)
target_link_libraries(${RESTORE_TEST03} ${BACKUP_LIB} pthread)
add_dependencies(${RESTORE_TEST03} link-backup-header link-backup-lib)
