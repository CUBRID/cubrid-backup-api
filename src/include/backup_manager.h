#ifndef _BACKUP_MANAGER_H_
#define _BACKUP_MANAGER_H_

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include "backup_common.h"

typedef struct backup_option BACKUP_OPTION;
struct backup_option
{
    bool remove_archive;
    bool sa_mode;
    bool no_check;
    int thread_count;
    bool compress;
    bool except_active_log;
    int sleep_msecs;
};

typedef struct restore_option RESTORE_OPTION;
struct restore_option
{
    bool partial_recovery;
    bool use_database_location_path;
};

typedef struct backup_manager BACKUP_MANAGER;
struct backup_manager
{
    char cubrid_home[PATH_MAX];
    char backup_home[PATH_MAX];

    int io_size; /* PAGE_SIZE * 8 */

    BACKUP_OPTION default_backup_option;
    RESTORE_OPTION default_restore_option;

    int log_fd;
};

extern BACKUP_MANAGER* backup_mgr;

int start_backup_manager (void);
int stop_backup_manager (void);
int validate_dir (const char*);
int check_path_length_limit (const char*);

#endif
