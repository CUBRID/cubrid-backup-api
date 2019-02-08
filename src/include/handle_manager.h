#ifndef _HANDLE_MANAGER_H_
#define _HANDLE_MANAGER_H_

#include <pthread.h>
#include <signal.h>
#include "backup_manager.h"

/* The maximum length of database name is 17 in English. */
#define MAX_DB_NAME_LEN 17

typedef enum handle_type HANDLE_TYPE;
enum handle_type
{
    BACKUP_HANDLE_TYPE,
    RESTORE_HANDLE_TYPE
};

typedef enum thread_state THREAD_STATE;
enum thread_state
{
    THREAD_STATE_NO_SPAWN,
    THREAD_STATE_RUNNING,
    THREAD_STATE_EXIT,
    THREAD_STATE_EXIT_WITH_ERROR
};

typedef enum backup_level BACKUP_LEVEL;
enum backup_level
{
    BACKUP_FULL_LEVEL = 0,
    BACKUP_BIG_INCREMENT_LEVEL,
    BACKUP_SMALL_INCREMENT_LEVEL
};

typedef struct backup_handle BACKUP_HANDLE;
struct backup_handle
{
    pthread_t backup_thread;
    pthread_mutex_t backup_mutex;

    THREAD_STATE backup_thread_state;

    bool is_cancel;

    BACKUP_LEVEL backup_level;

    int fifo_fd;
    char fifo_path[PATH_MAX];

    char db_name[MAX_DB_NAME_LEN + 1];
};

typedef struct restore_handle RESTORE_HANDLE;
struct restore_handle
{
    pthread_mutex_t restore_mutex;

    int restore_type;

    BACKUP_LEVEL backup_level;

    int restore_fd;
    char backup_file_path[PATH_MAX];

    char db_name[MAX_DB_NAME_LEN + 1];
};

typedef struct handle_manager HANDLE_MANAGER;
struct handle_manager
{
    BACKUP_HANDLE backup_handle;
    RESTORE_HANDLE restore_handle;
};

extern HANDLE_MANAGER* handle_mgr;

int start_handle_manager (void);
int stop_handle_manager (void);
int alloc_handle (HANDLE_TYPE, void**);
int free_handle (HANDLE_TYPE, void*);
int validate_handle (HANDLE_TYPE, void*);
int set_thread_state (HANDLE_TYPE, void*, THREAD_STATE);

#endif
