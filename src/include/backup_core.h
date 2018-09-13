#ifndef _BACKUP_CORE_H_
#define _BACKUP_CORE_H_

#include "handle_manager.h"

#define SIGKILL 9

typedef enum backup_api_state BACKUP_API_STATE;
enum backup_api_state
{
    BACKUP_API_STATE_NOT_READY,
    BACKUP_API_STATE_INITIALIZING,
    BACKUP_API_STATE_READY,
    BACKUP_API_STATE_BACKUP_SERVICE,
    BACKUP_API_STATE_RESTORE_SERVICE,
    BACKUP_API_STATE_FINALIZING
};

typedef enum func_call FUNC_CALL;
enum func_call
{
    FUNC_CALL_INITIALIZE,
    FUNC_CALL_FINALIZE,
    FUNC_CALL_BACKUP_BEGIN,
    FUNC_CALL_BACKUP_END,
    FUNC_CALL_BACKUP_READ,
    FUNC_CALL_RESTORE_BEGIN,
    FUNC_CALL_RESTORE_END,
    FUNC_CALL_RESTORE_WRITE
};

extern pthread_once_t backup_api_once_initialize;
extern pthread_once_t backup_api_once_finalize;

void initialize_backup_api (void);
void finalize_backup_api (void);
int check_api_call_sequence (FUNC_CALL);
int transit_backup_api_state (BACKUP_API_STATE, BACKUP_API_STATE);
int begin_backup (CUBRID_BACKUP_INFO*, void**);
int end_backup (BACKUP_HANDLE*);
int begin_restore (CUBRID_RESTORE_INFO*, void**);
int end_restore (RESTORE_HANDLE*);
int read_backup_data (BACKUP_HANDLE*, void*, unsigned int, unsigned int*);
int write_backup_data (RESTORE_HANDLE*, int, void*, unsigned int);

#endif
