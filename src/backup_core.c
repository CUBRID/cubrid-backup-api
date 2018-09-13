#include <sys/wait.h>
#include <errno.h>
#include "backup_api.h"
#include "backup_core.h"
#include "backup_manager.h"
#include "handle_manager.h"

pthread_once_t backup_api_once_initialize = PTHREAD_ONCE_INIT;
pthread_once_t backup_api_once_finalize   = PTHREAD_ONCE_INIT;

pthread_mutex_t backup_api_state_mutex;
BACKUP_API_STATE backup_api_state = BACKUP_API_STATE_NOT_READY;

void initialize_backup_api (void)
{
    pthread_mutex_init (&backup_api_state_mutex, NULL);

    backup_api_once_finalize = PTHREAD_ONCE_INIT;
}

void finalize_backup_api (void)
{
    pthread_mutex_destroy (&backup_api_state_mutex);

    backup_api_once_initialize = PTHREAD_ONCE_INIT;

    backup_api_state = BACKUP_API_STATE_NOT_READY;
}

int transit_backup_api_state (BACKUP_API_STATE state_now, BACKUP_API_STATE state_where)
{
    int state = 0;

    if (IS_FAILURE (pthread_mutex_lock (&backup_api_state_mutex)))
    {
        goto error;
    }

    state = 1;

    if (backup_api_state != state_now)
    {
        goto error;
    }

    backup_api_state = state_where;

    if (IS_FAILURE (pthread_mutex_unlock (&backup_api_state_mutex)))
    {
        goto error;
    }

    return SUCCESS;

error:

    switch (state)
    {
        case 1:
            pthread_mutex_unlock (&backup_api_state_mutex);
        default:
            break;
    }

    return FAILURE;
}

static
int transit_backup_api_state_to_end (void)
{
    int state = 0;

    if (IS_FAILURE (pthread_mutex_lock (&backup_api_state_mutex)))
    {
        goto error;
    }

    state = 1;

    if (backup_api_state == BACKUP_API_STATE_NOT_READY ||
        backup_api_state == BACKUP_API_STATE_INITIALIZING)
    {
        goto error;
    }

    backup_api_state = BACKUP_API_STATE_FINALIZING;

    if (IS_FAILURE (pthread_mutex_unlock (&backup_api_state_mutex)))
    {
        goto error;
    }

    return SUCCESS;

error:

    switch (state)
    {
        case 1:
            pthread_mutex_unlock (&backup_api_state_mutex);
        default:
            break;
    }

    return FAILURE;
}

static
int check_backup_api_state (BACKUP_API_STATE state_now)
{
    if (IS_FAILURE (pthread_mutex_lock (&backup_api_state_mutex)))
    {
        goto error;
    }

    if (backup_api_state != state_now)
    {
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_unlock (&backup_api_state_mutex)))
    {
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

int check_api_call_sequence (FUNC_CALL func_call)
{
    switch (func_call)
    {
        case FUNC_CALL_INITIALIZE:
            if (IS_FAILURE (transit_backup_api_state (BACKUP_API_STATE_NOT_READY, BACKUP_API_STATE_INITIALIZING)))
            {
                goto error;
            }

            break;

        case FUNC_CALL_FINALIZE:
            if (IS_FAILURE (transit_backup_api_state_to_end ()))
            {
                goto error;
            }

            break;

        case FUNC_CALL_BACKUP_BEGIN:
            if (IS_FAILURE (transit_backup_api_state (BACKUP_API_STATE_READY, BACKUP_API_STATE_BACKUP_SERVICE)))
            {
                goto error;
            }

            break;

        case FUNC_CALL_BACKUP_END:
        case FUNC_CALL_BACKUP_READ:
            if (IS_FAILURE (check_backup_api_state (BACKUP_API_STATE_BACKUP_SERVICE)))
            {
                goto error;
            }

            break;

        case FUNC_CALL_RESTORE_BEGIN:
            if (IS_FAILURE (transit_backup_api_state (BACKUP_API_STATE_READY, BACKUP_API_STATE_RESTORE_SERVICE)))
            {
                goto error;
            }

            break;

        case FUNC_CALL_RESTORE_END:
        case FUNC_CALL_RESTORE_WRITE:
            if (IS_FAILURE (check_backup_api_state (BACKUP_API_STATE_RESTORE_SERVICE)))
            {
                goto error;
            }

            break;

        default:
            goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
int check_backup_info (CUBRID_BACKUP_INFO* backup_info)
{
    if (backup_info->backup_level < BACKUP_FULL_LEVEL ||
        backup_info->backup_level > BACKUP_SMALL_INCREMENT_LEVEL)
    {
        goto error;
    }

    if (IS_NULL (backup_info->db_name))
    {
        goto error;
    }

    if (strlen (backup_info->db_name) > MAX_DB_NAME_LEN)
    {
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
int set_backup_info (CUBRID_BACKUP_INFO* backup_info, BACKUP_HANDLE* backup_handle)
{
    if (IS_FAILURE (check_backup_info (backup_info)))
    {
        goto error;
    }

    backup_handle->backup_level = backup_info->backup_level;

    snprintf (backup_handle->db_name, MAX_DB_NAME_LEN + 1, "%s", backup_info->db_name);

    return SUCCESS;

error:

    return FAILURE;
}

static
int check_restore_info (CUBRID_RESTORE_INFO* restore_info)
{
    if (/* restore_info->restore_type != RESTORE_TO_DB || */
        restore_info->restore_type != RESTORE_TO_FILE)
    {
        goto error;
    }

    if (restore_info->backup_level < BACKUP_FULL_LEVEL ||
        restore_info->backup_level > BACKUP_SMALL_INCREMENT_LEVEL)
    {
        goto error;
    }

#if 0
    if (restore_handle->up_to_date != NULL)
    {
        // check format, now not supported
    }
#endif

    if (restore_info->restore_type == RESTORE_TO_FILE)
    {
        if (IS_NULL (restore_info->backup_file_path))
        {
            goto error;
        }

        if (IS_FAILURE (validate_dir (restore_info->backup_file_path)))
        {
            goto error;
        }
    }

    if (IS_NULL (restore_info->db_name))
    {
        goto error;
    }

    if (strlen (restore_info->db_name) > MAX_DB_NAME_LEN)
    {
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
int set_restore_info (CUBRID_RESTORE_INFO* restore_info, RESTORE_HANDLE* restore_handle)
{
    if (IS_FAILURE (check_restore_info (restore_info)))
    {
        goto error;
    }

    restore_handle->restore_type = restore_info->restore_type;

    restore_handle->backup_level = restore_info->backup_level;

    snprintf (restore_handle->backup_file_path, PATH_MAX, "%s", restore_info->backup_file_path);

    snprintf (restore_handle->db_name, MAX_DB_NAME_LEN + 1, "%s", restore_info->db_name);

    return SUCCESS;

error:

    return FAILURE;
}

static
int make_fifo (HANDLE_TYPE handle_type, void* handle)
{
    BACKUP_HANDLE* backup_handle;

    if (handle_type == BACKUP_HANDLE_TYPE)
    {
        backup_handle = (BACKUP_HANDLE *)handle;

        /* ex) demodb_bk0v000 */
        snprintf (backup_handle->fifo_path, PATH_MAX, "%s/%s_bk%dv000", backup_mgr->backup_home,
                                                                        backup_handle->db_name,
                                                                        backup_handle->backup_level);

        if (IS_FAILURE (check_path_length_limit (backup_handle->fifo_path)))
        {
            goto error;
        }

        if (IS_SUCCESS (access (backup_handle->fifo_path, F_OK)))
        {
            if (IS_FAILURE (unlink (backup_handle->fifo_path)))
            {
                goto error;
            }
        }

        if (IS_FAILURE (mkfifo (backup_handle->fifo_path, S_IRUSR|S_IWUSR)))
        {
            goto error;
        }
    }
    else
    {
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
int open_fifo (HANDLE_TYPE handle_type, void* handle)
{
    BACKUP_HANDLE* backup_handle;

    if (IS_FAILURE (make_fifo (handle_type, handle)))
    {
        goto error;
    }

    if (handle_type == BACKUP_HANDLE_TYPE)
    {
        backup_handle = (BACKUP_HANDLE *)handle;

        backup_handle->fifo_fd = open (backup_handle->fifo_path, O_RDONLY | O_NONBLOCK);

        if (backup_handle->fifo_fd == -1)
        {
            goto error;
        }
    }
    else
    {
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
int remove_fifo (HANDLE_TYPE handle_type, void* handle)
{
    BACKUP_HANDLE* backup_handle;

    if (handle_type == BACKUP_HANDLE_TYPE)
    {
        backup_handle = (BACKUP_HANDLE *)handle;

        if (IS_FAILURE (unlink (backup_handle->fifo_path)))
        {
            goto error;
        }

        backup_handle->fifo_path[0] = '\0';
    }
    else
    {
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
int close_fifo (HANDLE_TYPE handle_type, void* handle)
{
    BACKUP_HANDLE* backup_handle;

    if (handle_type == BACKUP_HANDLE_TYPE)
    {
        backup_handle = (BACKUP_HANDLE *)handle;

        if (backup_handle->fifo_fd != -1)
        {
            close (backup_handle->fifo_fd);
        }

        backup_handle->fifo_fd = -1;
    }
    else
    {
        goto error;
    }

    if (IS_FAILURE (remove_fifo (handle_type, handle)))
    {
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
int execute_cubrid_backupdb (BACKUP_HANDLE* backup_handle)
{
    BACKUP_OPTION* backup_opt;

    char* argv[16];
    int idx = 0;

    char cubrid[PATH_MAX];
    char cub_admin[PATH_MAX];

    char thread_count[11];
    char sleep_msecs[25];

    char* db_name;

    backup_opt = &backup_mgr->default_backup_option;

    snprintf (cubrid, PATH_MAX, "%s/bin/cubrid", backup_mgr->cubrid_home);

    if (IS_FAILURE (check_path_length_limit (cubrid)))
    {
        goto error;
    }

    argv[idx ++] = cubrid;

    argv[idx ++] = "backupdb";

    /* --destination-path */
    argv[idx ++] = "-D";
    argv[idx ++] = backup_handle->fifo_path;

    /* --remove-archive */
    if (backup_opt->remove_archive == true)
    {
        argv[idx ++] = "-r";
    }

    /* --level */
    argv[idx ++] = "-l";

    if (backup_handle->backup_level == BACKUP_FULL_LEVEL)
    {
        argv[idx ++] = "0";
    }
    else if (backup_handle->backup_level == BACKUP_BIG_INCREMENT_LEVEL)
    {
        argv[idx ++] = "1";
    }
    else if (backup_handle->backup_level == BACKUP_SMALL_INCREMENT_LEVEL)
    {
        argv[idx ++] = "2";
    }

    /* --SA-mode */
    if (backup_opt->sa_mode == true)
    {
        argv[idx ++] = "-S";
    }

    /* --no-check */
    if (backup_opt->no_check == true)
    {
        argv[idx ++] = "--no-check";
    }

    /* --thread-count */
    if (backup_opt->thread_count != 0)
    {
        snprintf (thread_count, 11, "%d", backup_opt->thread_count);

        argv[idx ++] = "-t";
        argv[idx ++] = thread_count;
    }

    /* --compress */
    if (backup_opt->compress == true)
    {
        argv[idx ++] = "-z";
    }

    /* --except-active-log */
    if (backup_opt->except_active_log == true)
    {
        argv[idx ++] = "-e";
    }

    /* --sleep-msecs */
    if (backup_opt->sleep_msecs != 0)
    {
        snprintf (sleep_msecs, 25, "--sleep-msecs=%d", backup_opt->sleep_msecs);

        argv[idx ++] = sleep_msecs;
    }

    db_name = backup_handle->db_name;

    argv[idx ++] = db_name;

    argv[idx] = '\0';

    snprintf (cub_admin, PATH_MAX, "%s/bin/cub_admin", backup_mgr->cubrid_home);

    if (IS_FAILURE (check_path_length_limit (cub_admin)))
    {
        goto error;
    }

    if (-1 == execv (cub_admin, (char * const *)argv))
    {
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
int check_backup_process_status (BACKUP_HANDLE* backup_handle, pid_t backup_pid)
{
    sigset_t sa_mask;
    struct timespec wait_timeout;

    int status;

    sigemptyset (&sa_mask);
    sigaddset (&sa_mask, SIGCHLD);
 
    wait_timeout.tv_sec  = 1;
    wait_timeout.tv_nsec = 0;

    while (true)   
    {
        if (-1 == sigtimedwait (&sa_mask, NULL, &wait_timeout))
        {
            /* timeout */
            if (errno == EAGAIN && backup_handle->is_cancel != true)
            {
                continue;
            }

            kill (backup_pid, SIGKILL);

            break;
        }
        else
        {
            if (-1 == waitpid (backup_pid, &status, 0))
            {
                goto error;
            }

            /* backup process(cub_admin) return or exit () */
            if (WIFEXITED (status))
            {
                /*
                 * backup process return value:
                 * 0 - backup success
                 * 1 - backup failure
                 */
                if (WEXITSTATUS(status))
                {
                    goto error;
                }
            }
            /* backup process is dead by signal */
            else if (WIFSIGNALED(status)) /* signal */
            {
                goto error;
            }
            /* backup process is stopped */
            else if (WIFSTOPPED(status))
            {
                kill (backup_pid, SIGKILL);

                goto error;
            }

            break;
        }
    }

    return SUCCESS;

error:

    waitpid (backup_pid, NULL, WNOHANG);

    return FAILURE;
}

static
void* execute_backup (void* handle)
{
    BACKUP_HANDLE* backup_handle;
    pid_t backup_pid;

    if (IS_NULL (handle))
    {
        goto error;
    }

    backup_handle = (BACKUP_HANDLE *)handle;

    set_thread_state (BACKUP_HANDLE_TYPE, backup_handle, THREAD_STATE_RUNNING);

    backup_pid = fork ();

    if (backup_pid == -1)
    {
        goto error;
    }
    else if (backup_pid == 0) /* child process */
    {
        if (IS_FAILURE (execute_cubrid_backupdb (backup_handle)))
        {
            goto error;
        }
    }
    else /* parent process */
    {
        if (IS_FAILURE (check_backup_process_status (backup_handle, backup_pid)))
        {
            goto error;
        }
    }

    set_thread_state (BACKUP_HANDLE_TYPE, backup_handle, THREAD_STATE_EXIT);

    pthread_exit (NULL);

error:

    set_thread_state (BACKUP_HANDLE_TYPE, backup_handle, THREAD_STATE_EXIT_WITH_ERROR);

    pthread_exit (NULL);
}

int begin_backup (CUBRID_BACKUP_INFO* backup_info, void** handle)
{
    BACKUP_HANDLE* backup_handle;

    int state = 0;

    if (IS_NULL (backup_info) || IS_NULL (handle))
    {
        goto error;
    }

    if (IS_FAILURE (alloc_handle (BACKUP_HANDLE_TYPE, (void **)&backup_handle)))
    {
        goto error;
    }

    state = 1;

    if (IS_FAILURE (set_backup_info (backup_info, backup_handle)))
    {
        goto error;
    }

    if (IS_FAILURE (open_fifo (BACKUP_HANDLE_TYPE, backup_handle)))
    {
        goto error;
    }

    state = 2;

    if (IS_FAILURE (pthread_create (&backup_handle->backup_thread, NULL, execute_backup, (void *)backup_handle)))
    {
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_unlock (&backup_handle->backup_mutex)))
    {
        goto error;
    }

    *(BACKUP_HANDLE **)handle = backup_handle;

    return SUCCESS;

error:

    switch (state)
    {
        case 2:
            backup_handle->is_cancel = true;
            pthread_join (backup_handle->backup_thread, NULL);
            close_fifo (BACKUP_HANDLE_TYPE, backup_handle);
        case 1:
            free_handle (BACKUP_HANDLE_TYPE, backup_handle);
        default:
            break;
    }

    return FAILURE;
}

int end_backup (BACKUP_HANDLE* backup_handle)
{
    int state = 0;

    if (IS_NULL (backup_handle))
    {
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_lock (&backup_handle->backup_mutex)))
    {
        goto error;
    }

    state = 1;

    if (IS_FAILURE (validate_handle (BACKUP_HANDLE_TYPE, backup_handle)))
    {
        goto error;
    }

    if (backup_handle->backup_thread_state == THREAD_STATE_RUNNING)
    {
        backup_handle->is_cancel = true;

        if (IS_FAILURE (pthread_join (backup_handle->backup_thread, NULL)))
        {
            goto error;
        }
    }

    if (IS_FAILURE (close_fifo (BACKUP_HANDLE_TYPE, backup_handle)))
    {
        goto error;
    }

    if (IS_FAILURE (free_handle (BACKUP_HANDLE_TYPE, backup_handle)))
    {
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_unlock (&backup_handle->backup_mutex)))
    {
        goto error;
    }

    return SUCCESS;

error:

    switch (state)
    {
        case 1:
            pthread_mutex_unlock (&backup_handle->backup_mutex);
        default:
            break;
    }

    return FAILURE;
}

static
int read_fifo (int fifo_fd, int io_size, char* buffer, int* read_len)
{
    int retval;
    int read_size;

    fd_set read_fds;

    FD_ZERO (&read_fds);
    FD_SET (fifo_fd, &read_fds);

    struct timeval wait_timeout;

    wait_timeout.tv_sec  = 2;
    wait_timeout.tv_usec = 0;

    retval = select (fifo_fd + 1, &read_fds, NULL, NULL, &wait_timeout);

    if (retval == -1)
    {
        goto error;
    }
    else if (retval == 0)
    {
        *read_len = 0;
    }
    else
    {
        if (FD_ISSET (fifo_fd, &read_fds))
        {
            read_size = read (fifo_fd, buffer, io_size);

            if (read_size == -1)
            {
                goto error;
            }

            *read_len = read_size;
        }
        else
        {
            goto error;
        }
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
int read_data (BACKUP_HANDLE* backup_handle, char* buffer, unsigned int buffer_size, unsigned int* data_len)
{
    int io_size = 0;
    int read_len = 0;
    int total_read_len = 0;
    int read_count = 0;
    int i;

    if (backup_handle->fifo_fd == -1)
    {
        goto error;
    }

    io_size = backup_mgr->io_size;

    read_count = buffer_size / io_size;

    for (i = 0; i < read_count; i ++)
    {
        if (backup_handle->backup_thread_state == THREAD_STATE_EXIT_WITH_ERROR)
        {
            goto error;
        }

        if (IS_FAILURE (read_fifo (backup_handle->fifo_fd, io_size, buffer + total_read_len, &read_len)))
        {
            goto error;
        }

        if (read_len == 0)
        {
            if (backup_handle->backup_thread_state == THREAD_STATE_EXIT)
            {
                break;
            }
        }

        total_read_len += read_len;
    }

    io_size = buffer_size % io_size;

    if (io_size != 0)
    {
        if (backup_handle->backup_thread_state == THREAD_STATE_EXIT_WITH_ERROR)
        {
            goto error;
        }

        if (IS_FAILURE (read_fifo (backup_handle->fifo_fd, io_size, buffer + total_read_len, &read_len)))
        {
            goto error;
        }

        total_read_len += read_len;
    }

    *data_len = total_read_len;

    return SUCCESS;

error:

    return FAILURE;
}

int read_backup_data (BACKUP_HANDLE* backup_handle, void* buffer, unsigned int buffer_size, unsigned int* data_len)
{
    int state = 0;

    if (IS_NULL (backup_handle) || IS_NULL (buffer) || IS_ZERO (buffer_size) || IS_NULL (data_len))
    {
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_lock (&backup_handle->backup_mutex)))
    {
        goto error;
    }

    state = 1;

    if (IS_FAILURE (validate_handle (BACKUP_HANDLE_TYPE, backup_handle)))
    {
        goto error;
    }

    if (IS_FAILURE (read_data (backup_handle, buffer, buffer_size, data_len)))
    {
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_unlock (&backup_handle->backup_mutex)))
    {
        goto error;
    }

    return SUCCESS;

error:

    switch (state)
    {
        case 1:
            pthread_mutex_unlock (&backup_handle->backup_mutex);
        default:
            break;
    }

    return FAILURE;
}


static
int open_restore_file (RESTORE_HANDLE* restore_handle)
{
    char restore_file[PATH_MAX];

    /* ex) demodb_bk0v000 */
    snprintf (restore_file, PATH_MAX, "%s/%s_bk%dv000", restore_handle->backup_file_path,
                                                        restore_handle->db_name,
                                                        restore_handle->backup_level);

    if (IS_FAILURE (check_path_length_limit (restore_file)))
    {
        goto error;
    }

    if (IS_SUCCESS (access (restore_file, F_OK)))
    {
        goto error;
    }

    restore_handle->restore_fd = open (restore_file, O_CREAT | O_WRONLY | O_CLOEXEC, S_IRUSR);

    if (restore_handle->restore_fd == -1)
    {
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
int close_restore_file (RESTORE_HANDLE* restore_handle)
{
    if (restore_handle->restore_fd != -1)
    {
        close (restore_handle->restore_fd);

        restore_handle->restore_fd = -1;

        restore_handle->backup_file_path[0] = '\0';
    }

    return SUCCESS;
}

static
int execute_restore_to_file (RESTORE_HANDLE* restore_handle)
{
    if (IS_FAILURE (open_restore_file (restore_handle)))
    {
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

int begin_restore (CUBRID_RESTORE_INFO* restore_info, void** handle)
{
    RESTORE_HANDLE* restore_handle;

    int state = 0;

    if (IS_NULL (restore_info) || IS_NULL (handle))
    {
        goto error;
    }

    if (IS_FAILURE (alloc_handle (RESTORE_HANDLE_TYPE, (void **)&restore_handle)))
    {
        goto error;
    }

    state = 1;

    if (IS_FAILURE (set_restore_info (restore_info, restore_handle)))
    {
        goto error;
    }

    if (restore_info->restore_type == RESTORE_TO_DB)
    {
        /* Not supported yet */
    }
    else if (restore_info->restore_type == RESTORE_TO_FILE)
    {
        if (IS_FAILURE (execute_restore_to_file (restore_handle)))
        {
            goto error;
        }
    }

    if (IS_FAILURE (pthread_mutex_unlock (&restore_handle->restore_mutex)))
    {
        goto error;
    }

    *(RESTORE_HANDLE **)handle = restore_handle;

    return SUCCESS;

error:

    switch (state)
    {
        case 1:
            free_handle (RESTORE_HANDLE_TYPE, restore_handle);
        default:
            break;
    }

    return FAILURE;
}

int end_restore (RESTORE_HANDLE* restore_handle)
{
    int state = 0;

    if (IS_NULL (restore_handle))
    {
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_lock (&restore_handle->restore_mutex)))
    {
        goto error;
    }

    state = 1;

    if (IS_FAILURE (validate_handle (RESTORE_HANDLE_TYPE, restore_handle)))
    {
        goto error;
    }

    if (restore_handle->restore_type == RESTORE_TO_DB)
    {
        /* Not supported yet */
    }
    else if (restore_handle->restore_type == RESTORE_TO_FILE)
    {
        close_restore_file (restore_handle);
    }

    if (IS_FAILURE (free_handle (RESTORE_HANDLE_TYPE, restore_handle)))
    {
        goto error;
    }

    return SUCCESS;

error:

    switch (state)
    {
        case 1:
            pthread_mutex_unlock (&restore_handle->restore_mutex);
        default:
            break;
    }

    return FAILURE;
}

static
int write_data_to_file (RESTORE_HANDLE* restore_handle, int backup_level, void* buffer, unsigned int data_len)
{
    int retval;

    if (restore_handle->backup_level != backup_level)
    {
        goto error;
    }

    retval = write (restore_handle->restore_fd, buffer, data_len);

    if (retval != data_len)
    {
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

int write_backup_data (RESTORE_HANDLE* restore_handle, int backup_level, void* buffer, unsigned int data_len)
{
    int state = 0;

    if (IS_NULL (restore_handle) || IS_NULL (buffer))
    {
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_lock (&restore_handle->restore_mutex)))
    {
        goto error;
    }

    state = 1;

    if (IS_FAILURE (validate_handle (RESTORE_HANDLE_TYPE, restore_handle)))
    {
        goto error;
    }

    if (backup_level < BACKUP_FULL_LEVEL ||
        backup_level > BACKUP_SMALL_INCREMENT_LEVEL)
    {
        goto error;
    }

    if (restore_handle->restore_type == RESTORE_TO_DB)
    {
        /* Not supported yet */
    }
    else if (restore_handle->restore_type == RESTORE_TO_FILE)
    {
        if (IS_FAILURE (write_data_to_file (restore_handle, backup_level, buffer, data_len)))
        {
            goto error;
        }
    }

    if (IS_FAILURE (pthread_mutex_unlock (&restore_handle->restore_mutex)))
    {
        goto error;
    }

    return SUCCESS;

error:

    switch (state)
    {
        case 1:
            pthread_mutex_unlock (&restore_handle->restore_mutex);
        default:
            break;
    }

    return FAILURE;
}
