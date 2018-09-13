#include <errno.h>
#include "handle_manager.h"

HANDLE_MANAGER handle_manager;

HANDLE_MANAGER* handle_mgr = &handle_manager;

static
int initialize_handle_manager (void)
{
    if (IS_FAILURE (pthread_mutex_init (&handle_mgr->backup_handle.backup_mutex, NULL)))
    {
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_init (&handle_mgr->restore_handle.restore_mutex, NULL)))
    {
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

    backup_handle->backup_level = BACKUP_FULL_LEVEL;

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
        goto error;
    }

    if (IS_FAILURE (free_handle (RESTORE_HANDLE_TYPE, &handle_mgr->restore_handle)))
    {
        goto error;
    }

    if (IS_FAILURE (finalize_handle_manager ()))
    {
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
            goto error;
        }

        state = 1;

        if (IS_FAILURE (initialize_backup_handle (backup_handle)))
        {
            goto error;
        }

        *(BACKUP_HANDLE **)handle = backup_handle;
    }
    else if (handle_type == RESTORE_HANDLE_TYPE)
    {
        restore_handle = &handle_mgr->restore_handle;

        if (IS_FAILURE (pthread_mutex_trylock (&restore_handle->restore_mutex)))
        {
            goto error;
        }

        state = 1;

        if (IS_FAILURE (initialize_restore_handle (restore_handle)))
        {
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
            goto error;
        }

        state = 1;

        if (IS_FAILURE (finalize_backup_handle (backup_handle)))
        {
            goto error;
        }

        if (IS_FAILURE (pthread_mutex_unlock (&backup_handle->backup_mutex)))
        {
            goto error;
        }
    }
    else if (handle_type == RESTORE_HANDLE_TYPE)
    {
        restore_handle = (RESTORE_HANDLE *)handle;

        retval = pthread_mutex_trylock (&restore_handle->restore_mutex);

        if (IS_FAILURE (retval) && retval != EBUSY)
        {
            goto error;
        }

        state = 1;

        if (IS_FAILURE (finalize_restore_handle (restore_handle)))
        {
            goto error;
        }

        if (IS_FAILURE (pthread_mutex_unlock (&restore_handle->restore_mutex)))
        {
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
            goto error;
        }
    }
    else if (handle_type == RESTORE_HANDLE_TYPE)
    {
        restore_handle = &handle_mgr->restore_handle;

        if (restore_handle != handle)
        {
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
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

