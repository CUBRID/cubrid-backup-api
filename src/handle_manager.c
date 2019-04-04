#include <errno.h>
#include "handle_manager.h"

HANDLE_MANAGER handle_manager;

HANDLE_MANAGER* handle_mgr = &handle_manager;

static
int initialize_handle_manager (void)
{
    if (IS_FAILURE (pthread_mutex_init (&handle_mgr->backup_handle.backup_mutex, NULL)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_init (&handle_mgr->restore_handle.restore_mutex, NULL)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
int finalize_handle_manager (void)
{
    pthread_mutex_destroy (&handle_mgr->backup_handle.backup_mutex);

    pthread_mutex_destroy (&handle_mgr->restore_handle.restore_mutex);

    return SUCCESS;
}

static
int initialize_backup_handle (BACKUP_HANDLE* backup_handle)
{
    backup_handle->backup_thread_state = THREAD_STATE_NO_SPAWN;

    backup_handle->is_cancel = false;

    backup_handle->backup_level   = BACKUP_FULL_LEVEL;
    backup_handle->remove_archive = false;
    backup_handle->sa_mode        = false;
    backup_handle->no_check       = false;
    backup_handle->compress       = false;

    backup_handle->fifo_fd = -1;
    backup_handle->fifo_path[0] = '\0';

    backup_handle->db_name[0] = '\0';

    return SUCCESS;
}

static
int finalize_backup_handle (BACKUP_HANDLE* backup_handle)
{
    if (backup_handle->backup_thread_state == THREAD_STATE_RUNNING)
    {
        backup_handle->is_cancel = true;

        // thread 상태 변경 후 create 하기 때문에 생성 실패시 hang 발생할 수 있다.
        // (생성 실패하였는데, 상태는 THREAD_STATE_RUNNING 이기 때문에)
        // 상태를 먼저 변경하는 이유는
        // cubrid_backup_finalize () 호출 시 thread 상태가 THREAD_STATE_RUNNING 로 바뀌기 전이라면,
        // 이 부분을 pass 하고, 내부 handle을 free하기 때문에
        // 서버에서 아래와 같은 에러가 발생한다.
        // ERROR: Destination-path does not exist or is not a directory.
        // 
        // 이는 handle에 있던 -D (fifo) 경로를 아래 initialize_backup_handle () 함수에서
        // 초기화하기 때문이다.
        // 이 구조적인 문제는 다음 버전에서 개선하기로 한다.
        pthread_join (backup_handle->backup_thread, NULL);
    }

    if (backup_handle->fifo_fd != -1)
    {
        close (backup_handle->fifo_fd);
        unlink (backup_handle->fifo_path);
    }

    initialize_backup_handle (backup_handle);

    return SUCCESS;
}

static
int initialize_restore_handle (RESTORE_HANDLE* restore_handle)
{
    restore_handle->restore_type = -1;

    restore_handle->backup_level = BACKUP_FULL_LEVEL;

    restore_handle->restore_fd = -1;
    restore_handle->backup_file_path[0] = '\0';

    restore_handle->db_name[0] = '\0';

    return SUCCESS;
}

static
int finalize_restore_handle (RESTORE_HANDLE* restore_handle)
{
    if (restore_handle->restore_fd != -1)
    {
        close (restore_handle->restore_fd);
    }

    initialize_restore_handle (restore_handle);

    return SUCCESS;
}

int start_handle_manager (void)
{
    if (IS_FAILURE (initialize_handle_manager ()))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

int stop_handle_manager (void)
{
    if (IS_FAILURE (free_handle (BACKUP_HANDLE_TYPE, &handle_mgr->backup_handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (free_handle (RESTORE_HANDLE_TYPE, &handle_mgr->restore_handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (finalize_handle_manager ()))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

int alloc_handle (HANDLE_TYPE handle_type, void** handle)
{
    BACKUP_HANDLE* backup_handle;
    RESTORE_HANDLE* restore_handle;

    int state = 0;

    if (handle_type == BACKUP_HANDLE_TYPE)
    {
        backup_handle = &handle_mgr->backup_handle;

        if (IS_FAILURE (pthread_mutex_trylock (&backup_handle->backup_mutex)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        state = 1;

        if (IS_FAILURE (initialize_backup_handle (backup_handle)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        *(BACKUP_HANDLE **)handle = backup_handle;
    }
    else if (handle_type == RESTORE_HANDLE_TYPE)
    {
        restore_handle = &handle_mgr->restore_handle;

        if (IS_FAILURE (pthread_mutex_trylock (&restore_handle->restore_mutex)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        state = 1;

        if (IS_FAILURE (initialize_restore_handle (restore_handle)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        *(RESTORE_HANDLE **)handle = restore_handle;
    }

    return SUCCESS;

error:

    switch (state)
    {
        case 1:
            if (handle_type == BACKUP_HANDLE_TYPE)
            {
                pthread_mutex_unlock (&backup_handle->backup_mutex);
            }
            else if (handle_type == RESTORE_HANDLE_TYPE)
            {
                pthread_mutex_unlock (&restore_handle->restore_mutex);
            }
        default:
            break;
    }

    return FAILURE;
}

int free_handle (HANDLE_TYPE handle_type, void* handle)
{
    BACKUP_HANDLE* backup_handle;
    RESTORE_HANDLE* restore_handle;

    int retval;
    int state = 0;

    if (handle_type == BACKUP_HANDLE_TYPE)
    {
        backup_handle = (BACKUP_HANDLE *)handle;

        retval = pthread_mutex_trylock (&backup_handle->backup_mutex);

        if (IS_FAILURE (retval) && retval != EBUSY)
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        state = 1;

        if (IS_FAILURE (finalize_backup_handle (backup_handle)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        if (IS_FAILURE (pthread_mutex_unlock (&backup_handle->backup_mutex)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }
    else if (handle_type == RESTORE_HANDLE_TYPE)
    {
        restore_handle = (RESTORE_HANDLE *)handle;

        retval = pthread_mutex_trylock (&restore_handle->restore_mutex);

        if (IS_FAILURE (retval) && retval != EBUSY)
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        state = 1;

        if (IS_FAILURE (finalize_restore_handle (restore_handle)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        if (IS_FAILURE (pthread_mutex_unlock (&restore_handle->restore_mutex)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }

    return SUCCESS;

error:

    switch (state)
    {
        case 1:
            if (handle_type == BACKUP_HANDLE_TYPE)
            {
                pthread_mutex_unlock (&backup_handle->backup_mutex);
            }
            else if (handle_type == RESTORE_HANDLE_TYPE)
            {
                pthread_mutex_unlock (&restore_handle->restore_mutex);
            }
        default:
            break;
    }

    return FAILURE;
}

int validate_handle (HANDLE_TYPE handle_type, void* handle)
{
    BACKUP_HANDLE* backup_handle;
    RESTORE_HANDLE* restore_handle;

    if (handle_type == BACKUP_HANDLE_TYPE)
    {
        backup_handle = &handle_mgr->backup_handle;

        if (backup_handle != handle)
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }
    else if (handle_type == RESTORE_HANDLE_TYPE)
    {
        restore_handle = &handle_mgr->restore_handle;

        if (restore_handle != handle)
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }

    return SUCCESS;

error:

    return FAILURE;
}

int set_thread_state (HANDLE_TYPE handle_type, void* handle, THREAD_STATE state_where)
{
    if (handle_type == BACKUP_HANDLE_TYPE)
    {
        ((BACKUP_HANDLE *)handle)->backup_thread_state = state_where;
    }
    else
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

