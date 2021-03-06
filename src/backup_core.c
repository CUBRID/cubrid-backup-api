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
    if (backup_api_once_initialize != PTHREAD_ONCE_INIT)
    {
        pthread_mutex_destroy (&backup_api_state_mutex);
        backup_api_once_initialize = PTHREAD_ONCE_INIT;
    }

    backup_api_state = BACKUP_API_STATE_NOT_READY;
}

int transit_backup_api_state (BACKUP_API_STATE state_now, BACKUP_API_STATE state_where)
{
    int state = 0;

    if (backup_api_once_initialize == PTHREAD_ONCE_INIT)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_lock (&backup_api_state_mutex)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    state = 1;

    if (backup_api_state != state_now)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    backup_api_state = state_where;

    if (IS_FAILURE (pthread_mutex_unlock (&backup_api_state_mutex)))
    {
        PRINT_LOG_ERR (ERR_INFO);
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

    if (backup_api_once_initialize == PTHREAD_ONCE_INIT)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_lock (&backup_api_state_mutex)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    state = 1;

    if (backup_api_state == BACKUP_API_STATE_NOT_READY ||
        backup_api_state == BACKUP_API_STATE_INITIALIZING)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    backup_api_state = BACKUP_API_STATE_FINALIZING;

    if (IS_FAILURE (pthread_mutex_unlock (&backup_api_state_mutex)))
    {
        PRINT_LOG_ERR (ERR_INFO);
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
    if (backup_api_once_initialize == PTHREAD_ONCE_INIT)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_lock (&backup_api_state_mutex)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (backup_api_state != state_now)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_unlock (&backup_api_state_mutex)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    return SUCCESS;

error:

    pthread_mutex_unlock (&backup_api_state_mutex);

    return FAILURE;
}

int check_api_call_sequence (FUNC_CALL func_call)
{
    switch (func_call)
    {
        case FUNC_CALL_INITIALIZE:
            if (IS_FAILURE (transit_backup_api_state (BACKUP_API_STATE_NOT_READY, BACKUP_API_STATE_INITIALIZING)))
            {
                PRINT_LOG_ERR (ERR_INFO);
                goto error;
            }

            break;

        case FUNC_CALL_FINALIZE:
            if (IS_FAILURE (transit_backup_api_state_to_end ()))
            {
                PRINT_LOG_ERR (ERR_INFO);
                goto error;
            }

            break;

        case FUNC_CALL_BACKUP_BEGIN:
            if (IS_FAILURE (transit_backup_api_state (BACKUP_API_STATE_READY, BACKUP_API_STATE_BACKUP_SERVICE)))
            {
                PRINT_LOG_ERR (ERR_INFO);
                goto error;
            }

            break;

        case FUNC_CALL_BACKUP_END:
        case FUNC_CALL_BACKUP_READ:
            if (IS_FAILURE (check_backup_api_state (BACKUP_API_STATE_BACKUP_SERVICE)))
            {
                PRINT_LOG_ERR (ERR_INFO);
                goto error;
            }

            break;

        case FUNC_CALL_RESTORE_BEGIN:
            if (IS_FAILURE (transit_backup_api_state (BACKUP_API_STATE_READY, BACKUP_API_STATE_RESTORE_SERVICE)))
            {
                PRINT_LOG_ERR (ERR_INFO);
                goto error;
            }

            break;

        case FUNC_CALL_RESTORE_END:
        case FUNC_CALL_RESTORE_WRITE:
            if (IS_FAILURE (check_backup_api_state (BACKUP_API_STATE_RESTORE_SERVICE)))
            {
                PRINT_LOG_ERR (ERR_INFO);
                goto error;
            }

            break;

        default:
            PRINT_LOG_ERR (ERR_INFO);
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
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    // -1: use cubrid_backup.conf configuration
    // 0 : unset
    // 1 : set
    if (backup_info->remove_archive < -1 ||
        backup_info->remove_archive > 1)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (backup_info->sa_mode < -1 ||
        backup_info->sa_mode > 1)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (backup_info->no_check < -1 ||
        backup_info->no_check > 1)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (backup_info->compress < -1 ||
        backup_info->compress > 1)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_NULL (backup_info->db_name))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (strlen (backup_info->db_name) > MAX_DB_NAME_LEN)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
int set_backup_info (CUBRID_BACKUP_INFO* backup_info, BACKUP_HANDLE* backup_handle)
{
    BACKUP_OPTION* backup_opt;

    if (IS_FAILURE (check_backup_info (backup_info)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    backup_opt = &backup_mgr->default_backup_option;

    backup_handle->backup_level = backup_info->backup_level;

    if (backup_info->remove_archive == -1)
    {
        backup_handle->remove_archive = backup_opt->remove_archive;
    }
    else
    {
        backup_handle->remove_archive = backup_info->remove_archive == 1 ? true : false;
    }

    if (backup_info->sa_mode == -1)
    {
        backup_handle->sa_mode = backup_opt->sa_mode;
    }
    else
    {
        backup_handle->sa_mode = backup_info->sa_mode == 1 ? true : false;
    }

    if (backup_info->no_check == -1)
    {
        backup_handle->no_check = backup_opt->no_check;
    }
    else
    {
        backup_handle->no_check = backup_info->no_check == 1 ? true : false;
    }

    if (backup_info->compress == -1)
    {
        backup_handle->compress = backup_opt->compress;
    }
    else
    {
        backup_handle->compress = backup_info->compress == 1 ? true : false;
    }

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
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (restore_info->backup_level < BACKUP_FULL_LEVEL ||
        restore_info->backup_level > BACKUP_SMALL_INCREMENT_LEVEL)
    {
        PRINT_LOG_ERR (ERR_INFO);
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
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        if (IS_FAILURE (validate_dir (restore_info->backup_file_path)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }

    if (IS_NULL (restore_info->db_name))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (strlen (restore_info->db_name) > MAX_DB_NAME_LEN)
    {
        PRINT_LOG_ERR (ERR_INFO);
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
        PRINT_LOG_ERR (ERR_INFO);
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
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        if (IS_SUCCESS (access (backup_handle->fifo_path, F_OK)))
        {
            if (IS_FAILURE (unlink (backup_handle->fifo_path)))
            {
                PRINT_LOG_ERR (ERR_INFO);
                goto error;
            }
        }

        if (IS_FAILURE (mkfifo (backup_handle->fifo_path, S_IRUSR|S_IWUSR)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
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

static
int open_fifo (HANDLE_TYPE handle_type, void* handle)
{
    BACKUP_HANDLE* backup_handle;

    if (IS_FAILURE (make_fifo (handle_type, handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (handle_type == BACKUP_HANDLE_TYPE)
    {
        backup_handle = (BACKUP_HANDLE *)handle;

        backup_handle->fifo_fd = open (backup_handle->fifo_path, O_RDONLY | O_NONBLOCK);

        if (backup_handle->fifo_fd == -1)
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
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

static
int remove_fifo (HANDLE_TYPE handle_type, void* handle)
{
    BACKUP_HANDLE* backup_handle;

    if (handle_type == BACKUP_HANDLE_TYPE)
    {
        backup_handle = (BACKUP_HANDLE *)handle;

        if (IS_FAILURE (unlink (backup_handle->fifo_path)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        backup_handle->fifo_path[0] = '\0';
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
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (remove_fifo (handle_type, handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);
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

//    char cubrid[PATH_MAX];
    char cub_admin[PATH_MAX];

    char thread_count[11];
    char sleep_msecs[25];

    char* db_name;

#if 0
    // for INFO
    char backup_cmd[8192] = {0};
    int cmd_len = 0;
    int i;
#endif

    backup_opt = &backup_mgr->default_backup_option;

/*
    snprintf (cubrid, PATH_MAX, "%s/bin/cubrid", backup_mgr->cubrid_home);

    if (IS_FAILURE (check_path_length_limit (cubrid)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }
*/
    //argv[idx ++] = cubrid;
    argv[idx ++] = "cubrid";

    argv[idx ++] = "backupdb";

    /* --destination-path */
    argv[idx ++] = "-D";
    argv[idx ++] = backup_handle->fifo_path;

    /* --remove-archive */
    if (backup_handle->remove_archive == true)
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
    if (backup_handle->sa_mode == true)
    {
        argv[idx ++] = "-S";
    }

    /* --no-check */
    if (backup_handle->no_check == true)
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
    if (backup_handle->compress == true)
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

    // tech 요청으로 cub_admin -> cubrid 로 변경: cubrid_utility.log 에 기록 남기기 위해
    //snprintf (cub_admin, PATH_MAX, "%s/bin/cub_admin", backup_mgr->cubrid_home);
    snprintf (cub_admin, PATH_MAX, "%s/bin/cubrid", backup_mgr->cubrid_home);

    if (IS_FAILURE (check_path_length_limit (cub_admin)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

#if 0
    for (i = 0; i < idx - 1; i ++)
    {
        cmd_len += snprintf (backup_cmd + cmd_len, "%s", argv[i]);
        backup_cmd[cmd_len] = ' '; // for space
        cmd_len ++;
    }

    backup_cmd[cmd_len - 1] = '\n';

    PRINT_LOG_INFO (backup_cmd);
    //printf ("%s\n", backup_cmd);
#endif

    if (-1 == execv (cub_admin, (char * const *)argv))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
void kill_process_group (pid_t pgid)
{
    struct sigaction act, old_act;

    act.sa_handler = SIG_IGN;
    sigemptyset (&act.sa_mask);
    act.sa_flags   = 0;

    //printf ("pid: %d, pgid: %d\n", getpid (), getpgid (getpid ()));

    // backup-api library 자체는 죽으면 안돼기 때문에
    sigaction (SIGTERM, &act, &old_act);

    killpg (pgid, SIGTERM);

    sigaction (SIGTERM, &old_act, NULL);

#if 0
    // FAILURE: Has been interrupted. 를 cubrid_utility.log 에 남기기 위해
    // SIGINT 를 사용하려고 했으나, cub_admin 에서 SIG_INT 핸들러를 등록해두어서
    // cub_admin 이 바로 죽지 않는다.
    // FAILURE: Has been interrupted. 로그를 남긴다는 것 자체가 정상 종료(예외처리) 했다는 것이다.
    sigaction (SIGINT, &act, &old_act);

    killpg (pgid, SIGINT);

    sigaction (SIGINT, &old_act, NULL);
#endif
}

void zombie_handler()
{
    /*
    int status;
    int spid;
    spid = wait(&status);
    printf("자식프로세스 wait 성공 \n");
    printf("================================\n");
    printf("PID         : %d\n", spid);
    printf("Exit Value  : %d\n", WEXITSTATUS(status));
    printf("Exit Stat   : %d\n", WIFEXITED(status));
    */
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

    // SIGCHLD에 대한 빈 핸들러를 등록한 이유는
    // 아래 sigtimedwait () 에서 cubrid의 종료를 감지하지 못하기 때문이다.
    // 따라서 cubrid 유틸은 <defunct> 상태가 되며,
    // cubrid_backup_read () 에서는 백업 쓰레드가 종료되었다는 사실을 모르게 된다.
    // 테스트 용으로 zombie_handler 를 등록했는데,
    // 원하는대로 cubrid 유틸 종료시 SIGCHLD를 던저주며, 이를 catch할 수 있게 됐다.
    // 이유는 아직 못찾았다.
    signal(SIGCHLD, (void *)zombie_handler);

    // 현재 cub_admin 을 직접 fork 하는 방식을 사용 중인데 log를 cubrid 유틸에서 남기기 때문에
    // 기술 본부에서 참조하는 cubrid_utility.log 로그에 백업 관련 기록이 남지 않는다.
    // 따러서 cubrid 를 fork 하는 방식으로 바꾸고 kill 시 다 죽이자 후손까지
    while (true)   
    {
        //sigemptyset (&sa_mask);
        //sigaddset (&sa_mask, SIGCHLD);
        //int ret;

        //ret = sigtimedwait (&sa_mask, NULL, &wait_timeout);
        //if (-1 == ret)
        if (-1 == sigtimedwait (&sa_mask, NULL, &wait_timeout))
        {
            /* timeout */
            if (errno == EAGAIN && backup_handle->is_cancel != true)
            {
                continue;
            }

            //printf ("must hit here 4\n");
            kill_process_group (getpgid (backup_pid));

            break;
        }
        else
        {
            //printf ("must hit here 1, ret val => %d, SIGCHLD => %d\n", ret, SIGCHLD);
            if (-1 == waitpid (backup_pid, &status, 0))
            {
                PRINT_LOG_ERR (ERR_INFO);
                goto error;
            }

            /* backup process(cubrid) return or exit () */
            if (WIFEXITED (status))
            {
                /*
                 * backup process return value:
                 * 0 - backup success
                 * 1 - backup failure
                 */
                if (WEXITSTATUS(status))
                {
                    PRINT_LOG_ERR (ERR_INFO);
                    goto error;
                }
            }
            /* backup process is dead by signal */
            else if (WIFSIGNALED(status)) /* signal */
            {
                PRINT_LOG_ERR (ERR_INFO);
                goto error;
            }
            /* backup process is stopped */
            else if (WIFSTOPPED(status))
            {
                kill_process_group (getpgid (backup_pid));

                PRINT_LOG_ERR (ERR_INFO);
                goto error;
            }

            break;
        }
    }

    //printf ("must hit here 2\n");
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
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    // for kill process group
    setpgid (getpid (), getpid ());

    backup_handle = (BACKUP_HANDLE *)handle;

    backup_pid = fork ();

    if (backup_pid == -1)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }
    else if (backup_pid == 0) /* child process */
    {
        if (IS_FAILURE (execute_cubrid_backupdb (backup_handle)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }
    else /* parent process */
    {
        if (IS_FAILURE (check_backup_process_status (backup_handle, backup_pid)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }

    // thread -> backup process
    // - thread 종료되었다고 ... backup_process 종료된건 아니다.
    // - 이 thread --fork--> backup process 구조는 검증 후 개선이 필요할 듯 하다.
    //   - 이 구조를 취한건 cubrid backup 유틸리티에서 자신을 실행한 thread id
    //     를 검증하는 부분이 있기 때문이다.
    //   - thread 로 백업 수행(libcubridsa.so 링크) 후 백업 수행 (백업 마다 thread 생성 or 한 쓰레드)
    //     cubrid_backup_finalize () 호출 시 boot_shutdown_client_at_exit () (atexit () 등록 됨)
    //     함수에서 coredump 발생
    //     이유는 백업은 thread 생성해서 수행되고, cubrid_backup_finalize () 호출은 main thread가
    //     호출하기 때문에 두 thread id가 달라서 발생된다고 분석된 상태.
    //     별도의 thread나 process를 생성해야 하는 이유는 사용자 레벨에서의 hang 방지.
    //printf ("must hit here 3\n");
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
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (alloc_handle (BACKUP_HANDLE_TYPE, (void **)&backup_handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    state = 1;

    if (IS_FAILURE (set_backup_info (backup_info, backup_handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (open_fifo (BACKUP_HANDLE_TYPE, backup_handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    state = 2;

    set_thread_state (BACKUP_HANDLE_TYPE, backup_handle, THREAD_STATE_RUNNING);

    if (IS_FAILURE (pthread_create (&backup_handle->backup_thread, NULL, execute_backup, (void *)backup_handle)))
    {
        set_thread_state (BACKUP_HANDLE_TYPE, backup_handle, THREAD_STATE_EXIT_WITH_ERROR);

        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_unlock (&backup_handle->backup_mutex)))
    {
        PRINT_LOG_ERR (ERR_INFO);
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
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    // mutex 잡기전에 handle validation을 먼저 수행하도록 변경한다.
    // 사유는 쓰레기 (NULL 아닌) 값을 handle로 전달할 경우
    // 1. hang
    // 2. seg fault
    // 발생할 수 있다.
    if (IS_FAILURE (validate_handle (BACKUP_HANDLE_TYPE, backup_handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_lock (&backup_handle->backup_mutex)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    state = 1;

    if (backup_handle->backup_thread_state == THREAD_STATE_RUNNING)
    {
        backup_handle->is_cancel = true;

        if (IS_FAILURE (pthread_join (backup_handle->backup_thread, NULL)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }

    if (IS_FAILURE (close_fifo (BACKUP_HANDLE_TYPE, backup_handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (free_handle (BACKUP_HANDLE_TYPE, backup_handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_unlock (&backup_handle->backup_mutex)))
    {
        PRINT_LOG_ERR (ERR_INFO);
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
        PRINT_LOG_ERR (ERR_INFO);
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
                PRINT_LOG_ERR (ERR_INFO);
                goto error;
            }

            *read_len = read_size;
        }
        else
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
int read_data (BACKUP_HANDLE* backup_handle, char* buffer, unsigned int buffer_size, unsigned int* data_len, bool* is_backup_end)
{
    int io_size = 0;
    int read_len = 0;
    int total_read_len = 0;
    int read_count = 0;
    int i;

    if (backup_handle->fifo_fd == -1)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    io_size = backup_mgr->io_size;

    read_count = buffer_size / io_size;

    for (i = 0; i < read_count; i ++)
    {
        if (backup_handle->backup_thread_state == THREAD_STATE_EXIT_WITH_ERROR)
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        if (IS_FAILURE (read_fifo (backup_handle->fifo_fd, io_size, buffer + total_read_len, &read_len)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        total_read_len += read_len;

        if (total_read_len == 0)
        {
            if (backup_handle->backup_thread_state == THREAD_STATE_EXIT)
            {
                *is_backup_end = true;

                break;
            }
        }
    }

    io_size = buffer_size % io_size;

    if (io_size != 0 && *is_backup_end != true) // refactoring
    {
        if (backup_handle->backup_thread_state == THREAD_STATE_EXIT_WITH_ERROR)
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        if (IS_FAILURE (read_fifo (backup_handle->fifo_fd, io_size, buffer + total_read_len, &read_len)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        total_read_len += read_len;

        if (total_read_len == 0)
        {
            if (backup_handle->backup_thread_state == THREAD_STATE_EXIT)
            {
                *is_backup_end = true;
            }
        }
    }

    *data_len = total_read_len;

    return SUCCESS;

error:

    return FAILURE;
}

int read_backup_data (BACKUP_HANDLE* backup_handle, void* buffer, unsigned int buffer_size, unsigned int* data_len, bool* is_backup_end)
{
    int state = 0;

    if (IS_NULL (backup_handle) || IS_NULL (buffer) || IS_ZERO (buffer_size) || IS_NULL (data_len))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (validate_handle (BACKUP_HANDLE_TYPE, backup_handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_lock (&backup_handle->backup_mutex)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    state = 1;

    if (IS_FAILURE (read_data (backup_handle, buffer, buffer_size, data_len, is_backup_end)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_unlock (&backup_handle->backup_mutex)))
    {
        PRINT_LOG_ERR (ERR_INFO);
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
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

#if 0
    // 파일이 존재하면 따로 검사하지 말고, overwrite 해버리자
    if (IS_SUCCESS (access (restore_file, F_OK)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }
#endif

    restore_handle->restore_fd = open (restore_file, O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, S_IRUSR | S_IWUSR);

    if (restore_handle->restore_fd == -1)
    {
        PRINT_LOG_ERR (ERR_INFO);
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
        PRINT_LOG_ERR (ERR_INFO);
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
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (alloc_handle (RESTORE_HANDLE_TYPE, (void **)&restore_handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    state = 1;

    if (IS_FAILURE (set_restore_info (restore_info, restore_handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (restore_info->restore_type == RESTORE_TO_DB)
    {
        /* Not supported yet */
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }
    else if (restore_info->restore_type == RESTORE_TO_FILE)
    {
        if (IS_FAILURE (execute_restore_to_file (restore_handle)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }

    if (IS_FAILURE (pthread_mutex_unlock (&restore_handle->restore_mutex)))
    {
        PRINT_LOG_ERR (ERR_INFO);
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
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (validate_handle (RESTORE_HANDLE_TYPE, restore_handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_lock (&restore_handle->restore_mutex)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    state = 1;

    if (restore_handle->restore_type == RESTORE_TO_DB)
    {
        /* Not supported yet */
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }
    else if (restore_handle->restore_type == RESTORE_TO_FILE)
    {
        close_restore_file (restore_handle);
    }

    if (IS_FAILURE (free_handle (RESTORE_HANDLE_TYPE, restore_handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);
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
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    retval = write (restore_handle->restore_fd, buffer, data_len);

    if (retval != data_len)
    {
        PRINT_LOG_ERR (ERR_INFO);
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
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (validate_handle (RESTORE_HANDLE_TYPE, restore_handle)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (pthread_mutex_lock (&restore_handle->restore_mutex)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    state = 1;

    if (backup_level < BACKUP_FULL_LEVEL ||
        backup_level > BACKUP_SMALL_INCREMENT_LEVEL)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (restore_handle->restore_type == RESTORE_TO_DB)
    {
        /* Not supported yet */
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }
    else if (restore_handle->restore_type == RESTORE_TO_FILE)
    {
        if (IS_FAILURE (write_data_to_file (restore_handle, backup_level, buffer, data_len)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }

    if (IS_FAILURE (pthread_mutex_unlock (&restore_handle->restore_mutex)))
    {
        PRINT_LOG_ERR (ERR_INFO);
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
