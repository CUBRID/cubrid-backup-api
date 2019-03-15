#include "backup_api.h"
#include "backup_core.h"
#include "backup_manager.h"
#include "handle_manager.h"

int cubrid_backup_initialize (void)
{
    int state = 0;

    if (IS_FAILURE (pthread_once (&backup_api_once_initialize, initialize_backup_api)))
    {
        goto error;
    }

    state = 1;

    if (IS_FAILURE (check_api_call_sequence (FUNC_CALL_INITIALIZE)))
    {
        goto error;
    }

    state = 2;

    if (IS_FAILURE (start_backup_manager ()))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    state = 3;

    if (IS_FAILURE (start_handle_manager ()))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (transit_backup_api_state (BACKUP_API_STATE_INITIALIZING, BACKUP_API_STATE_READY)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

#if 0
    PRINT_LOG_INFO ("cubrid_backup_initialize ()\n");
#endif

    return SUCCESS;

error:

    switch (state)
    {
        case 3:
            stop_handle_manager ();
        case 2:
            stop_backup_manager ();
        case 1:
            pthread_once (&backup_api_once_finalize, finalize_backup_api);
        default:
            break;
    }

    return FAILURE;
}

int cubrid_backup_finalize (void)
{
#if 0
    PRINT_LOG_INFO ("cubrid_backup_finalize ()\n");
#endif

    if (IS_FAILURE (check_api_call_sequence (FUNC_CALL_FINALIZE)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (stop_handle_manager ()))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (stop_backup_manager ()))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    pthread_once (&backup_api_once_finalize, finalize_backup_api);

    return SUCCESS;

error:

    pthread_once (&backup_api_once_finalize, finalize_backup_api);

    return FAILURE;
}

int cubrid_backup_begin (CUBRID_BACKUP_INFO* backup_info, void** backup_handle)
{
    int state = 0;

#if 0
    PRINT_LOG_INFO ("cubrid_backup_begin (), backup_level => %d, db_name => %s\n",
                    backup_info->backup_level,
                    backup_info->db_name);
#endif

    if (IS_FAILURE (check_api_call_sequence (FUNC_CALL_BACKUP_BEGIN)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    state = 1;

    if (IS_FAILURE (begin_backup (backup_info, backup_handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);

        PRINT_LOG_INFO ("cubrid_backup_begin (), backup_level => %d, db_name => %s\n",
                        backup_info->backup_level,
                        backup_info->db_name);

        goto error;
    }

#if 0
    PRINT_LOG_INFO ("cubrid_backup_begin (), backup_handle => %p\n", *backup_handle);
#endif

    return SUCCESS;

error:

    switch (state)
    {
        case 1:
            transit_backup_api_state (BACKUP_API_STATE_BACKUP_SERVICE, BACKUP_API_STATE_READY);
        default:
            break;
    }

    return FAILURE;
}

int cubrid_backup_end (void* backup_handle)
{
#if 0
    PRINT_LOG_INFO ("cubrid_backup_end (), backup_handle => %p\n", backup_handle);
#endif

    if (IS_FAILURE (check_api_call_sequence (FUNC_CALL_BACKUP_END)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (end_backup (backup_handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);

        PRINT_LOG_INFO ("cubrid_backup_end (), backup_handle => %p\n", backup_handle);

        goto error;
    }

    if (IS_FAILURE (transit_backup_api_state (BACKUP_API_STATE_BACKUP_SERVICE, BACKUP_API_STATE_READY)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

int cubrid_backup_read (void* backup_handle, void* buffer, unsigned int buffer_size, unsigned int* data_len)
{
#if 0
    PRINT_LOG_INFO ("cubrid_backup_read (), backup_handle => %p, buffer => %p, buffer_size => %d, data_len => %p\n",
                    backup_handle,
                    buffer,
                    buffer_size,
                    data_len);
#endif

    if (IS_FAILURE (check_api_call_sequence (FUNC_CALL_BACKUP_READ)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (read_backup_data (backup_handle, buffer, buffer_size, data_len)))
    {
        PRINT_LOG_ERR (ERR_INFO);

        PRINT_LOG_INFO ("cubrid_backup_read (), backup_handle => %p, buffer => %p, buffer_size => %d, data_len => %p\n",
                        backup_handle,
                        buffer,
                        buffer_size,
                        data_len);

        goto error;
    }

#if 0
    PRINT_LOG_INFO ("cubrid_backup_read (), data_len => %d\n", *data_len);
#endif

    return SUCCESS;

error:

    return FAILURE;
}

int cubrid_restore_begin (CUBRID_RESTORE_INFO* restore_info, void** restore_handle)
{
    int state = 0;

#if 0
    PRINT_LOG_INFO ("cubrid_restore_begin (), restore_type => %d, backup_level => %d, backup_file_path => %s, db_name => %s\n",
                    restore_info->restore_type,
                    restore_info->backup_level,
                    restore_info->backup_file_path,
                    restore_info->db_name);
#endif

    if (IS_FAILURE (check_api_call_sequence (FUNC_CALL_RESTORE_BEGIN)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    state = 1;

    if (IS_FAILURE (begin_restore (restore_info, restore_handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);

        PRINT_LOG_INFO ("cubrid_restore_begin (), restore_type => %d, backup_level => %d, backup_file_path => %s, db_name => %s\n",
                        restore_info->restore_type,
                        restore_info->backup_level,
                        restore_info->backup_file_path,
                        restore_info->db_name);

        goto error;
    }

#if 0
    PRINT_LOG_INFO ("cubrid_restore_begin (), restore_handle => %p\n", *restore_handle);
#endif

    return SUCCESS;

error:

    switch (state)
    {
        case 1:
            transit_backup_api_state (BACKUP_API_STATE_RESTORE_SERVICE, BACKUP_API_STATE_READY);
        default:
            break;
    }

    return FAILURE;
}

int cubrid_restore_end (void* restore_handle)
{
#if 0
    PRINT_LOG_INFO ("cubrid_restore_end (), restore_handle => %p\n", restore_handle);
#endif

    if (IS_FAILURE (check_api_call_sequence (FUNC_CALL_RESTORE_END)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (end_restore (restore_handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);

        PRINT_LOG_INFO ("cubrid_restore_end (), restore_handle => %p\n", restore_handle);

        goto error;
    }

    if (IS_FAILURE (transit_backup_api_state (BACKUP_API_STATE_RESTORE_SERVICE, BACKUP_API_STATE_READY)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

int cubrid_restore_write (void* restore_handle, int backup_level, void* buffer, unsigned int data_len)
{
#if 0
    PRINT_LOG_INFO ("cubrid_restore_write (), restore_handle => %p, backup_level => %d, buffer => %p, data_len => %d\n",
                    restore_handle,
                    backup_level,
                    buffer,
                    data_len);
#endif

    if (IS_FAILURE (check_api_call_sequence (FUNC_CALL_RESTORE_WRITE)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (write_backup_data (restore_handle, backup_level, buffer, data_len)))
    {
        PRINT_LOG_ERR (ERR_INFO);

        PRINT_LOG_INFO ("cubrid_restore_write (), restore_handle => %p, backup_level => %d, buffer => %p, data_len => %d\n",
                        restore_handle,
                        backup_level,
                        buffer,
                        data_len);

        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}
