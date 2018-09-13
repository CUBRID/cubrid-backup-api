#include <stdlib.h>
#include <regex.h>
#include <errno.h>
#include "backup_manager.h"

#define INT_MAX 2147483647

BACKUP_MANAGER backup_manager;

BACKUP_MANAGER* backup_mgr = &backup_manager;

static
int open_log_file (void)
{
    const char* LOG_FILE = ".cubrid_backup.log";

    backup_mgr->log_fd = open (LOG_FILE, O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (backup_mgr->log_fd == -1)
    {
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
int close_log_file (void)
{
    if (backup_mgr->log_fd != -1)
    {
        close (backup_mgr->log_fd);
    }

    backup_mgr->log_fd = -1;

    return SUCCESS;
}

int validate_dir (const char* path)
{
    struct stat path_stat;

    if (IS_NULL (path) || strlen (path) >= PATH_MAX)
    {
        goto error; 
    }

    if (IS_FAILURE (access (path, R_OK | W_OK | X_OK)))
    {
        goto error;
    }

    if (IS_FAILURE (stat (path, &path_stat)))
    {
        goto error;
    }
    else
    {
        if (!S_ISDIR (path_stat.st_mode))
        {
            goto error;
        }
    }

    return SUCCESS;

error:

    return FAILURE;
}

int remove_all_entries (const char* dir)
{
    return SUCCESS;
}

static
int remove_trailing_slash (char* path_buffer)
{
    int path_len;
    int i;

    if (IS_NULL (path_buffer))
    {
        goto error;
    }

    path_len = strlen (path_buffer);

    for (i = path_len - 1; i >= 0; i --)
    {
        if (path_buffer[i] == ' ')
        {
            continue;
        }
        else
        {
            if (path_buffer[i] == '/')
            {
                path_buffer[i] = '\0';
            }

            break;
        }
    }

    return SUCCESS;

error:

    return FAILURE;
}

int check_path_length_limit (const char* path)
{
    if (strlen (path) >= PATH_MAX - 1)
    {
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
int set_cubrid_home (void)
{
    char* cub_home;

    cub_home = getenv ("CUBRID");

    if (IS_FAILURE (validate_dir (cub_home)))
    {
        goto error;
    }

    snprintf (backup_mgr->cubrid_home, PATH_MAX, "%s", cub_home);

    remove_trailing_slash (backup_mgr->cubrid_home);

    return SUCCESS;

error:

    return FAILURE;
}

static
int make_backup_home (void)
{
    char backup_path[PATH_MAX];

    /* (1) $CUBRID/tmp */
    snprintf (backup_path, PATH_MAX, "%s/tmp", backup_mgr->cubrid_home);

    if (IS_SUCCESS (validate_dir (backup_path)))
    {

    }
    else
    {
        /* (2) $CUBRID_TMP */
        snprintf (backup_path, PATH_MAX, "%s", getenv ("CUBRID_TMP"));

        remove_trailing_slash (backup_path);

        if (IS_SUCCESS (validate_dir (backup_path)))
        {

        }
        else
        {
            /* (3) /tmp */
            snprintf (backup_path, PATH_MAX, "%s", "/tmp");

            if (IS_SUCCESS (validate_dir (backup_path)))
            {

            }
            else
            {
                goto error;
            }
        }
    }

    snprintf (backup_mgr->backup_home, PATH_MAX, "%s/%s", backup_path, ".cubrid_backup");

    if (IS_FAILURE (check_path_length_limit (backup_mgr->backup_home)))
    {
        goto error;
    }

    if (IS_FAILURE (mkdir (backup_mgr->backup_home, S_IRWXU)))
    {
        if (errno != EEXIST)
        {
            goto error;
        }
        else
        {
            if (IS_FAILURE (validate_dir (backup_mgr->backup_home)))
            {
                goto error;
            }
            else
            {
                if (IS_FAILURE (remove_all_entries (backup_mgr->backup_home)))
                {
                    goto error;
                }
            }
        }
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
int remove_backup_home (void)
{
    if (IS_FAILURE (remove_all_entries (backup_mgr->backup_home)))
    {
        goto error;
    }

    if (IS_FAILURE (rmdir (backup_mgr->backup_home)))
    {
        goto error;
    }

    backup_mgr->backup_home[0] = '\0';

    return SUCCESS;

error:

    return FAILURE;
}

static
int init_default_backup_option (void)
{
    BACKUP_OPTION* backup_opt;

    backup_opt = &backup_mgr->default_backup_option;

    backup_opt->remove_archive     = false;
    backup_opt->sa_mode            = false;
    backup_opt->no_check           = false; /* [M] */
    backup_opt->thread_count       = 0;     /* [M] */
    backup_opt->compress           = false; /* [M] */
    backup_opt->except_active_log  = false; /* [M] */
    backup_opt->sleep_msecs        = 0;     /* [M] */
 
    return SUCCESS;
}

static
int init_default_restore_option (void)
{
    RESTORE_OPTION* restore_opt;

    restore_opt = &backup_mgr->default_restore_option;

    restore_opt->partial_recovery           = false;
    restore_opt->use_database_location_path = false;

    return SUCCESS;
}

static
int set_bool_value (bool* dest, char* src)
{
    if (IS_ZERO (strncasecmp (src, "true", 5)))
    {
        *dest = true;

        goto end;
    }

    if (IS_ZERO (strncasecmp (src, "false", 6)))
    {
        *dest = false;

        goto end;
    }

    if (IS_ZERO (strncmp (src, "1", 2)))
    {
        *dest = true;

        goto end;
    }

    if (IS_ZERO (strncmp (src, "0", 2)))
    {
        *dest = false;

        goto end;
    }

    return FAILURE;

end:

    return SUCCESS;
}

static
int set_int_value (int* dest, char* src)
{
    int i;
    long long_val;

    for (i = 0; i < strlen (src); i ++);
    {
        if (src[i] < '0' || src[i] > '9')
        {
            goto error;
        }
    }

    long_val = strtol (src, NULL, 10);

    if (errno == ERANGE || long_val > INT_MAX)
    {
        goto error;
    }

    *dest = (int) long_val;

    return SUCCESS;

error:

    return FAILURE;
}

static
int set_backup_option (char* key, char* value)
{
    BACKUP_OPTION* backup_opt;

    backup_opt = &backup_mgr->default_backup_option;

    if (IS_ZERO (strncasecmp (key, "remove_archive", 15)))
    {
        if (IS_FAILURE (set_bool_value (&backup_opt->remove_archive, value)))
        {
            goto error;
        }
    }
    else if (IS_ZERO (strncasecmp (key, "sa_mode", 8)))
    {
        if (IS_FAILURE (set_bool_value (&backup_opt->sa_mode, value)))
        {
            goto error;
        }
    }
    else if (IS_ZERO (strncasecmp (key, "no_check", 9)))
    {
        if (IS_FAILURE (set_bool_value (&backup_opt->no_check, value)))
        {
            goto error;
        }
    }
    else if (IS_ZERO (strncasecmp (key, "thread_count", 13)))
    {
        if (IS_FAILURE (set_int_value (&backup_opt->thread_count, value)))
        {
            goto error;
        }
    }
    else if (IS_ZERO (strncasecmp (key, "compress", 9)))
    {
        if (IS_FAILURE (set_bool_value (&backup_opt->compress, value)))
        {
            goto error;
        }
    }
    else if (IS_ZERO (strncasecmp (key, "except_active_log", 18)))
    {
        if (IS_FAILURE (set_bool_value (&backup_opt->except_active_log, value)))
        {
            goto error;
        }
    }
    else if (IS_ZERO (strncasecmp (key, "sleep_msecs", 12)))
    {
        if (IS_FAILURE (set_int_value (&backup_opt->sleep_msecs, value)))
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
int set_restore_option (char* key, char* value)
{
    RESTORE_OPTION* restore_opt;

    restore_opt = &backup_mgr->default_restore_option;

    if (0 == strncasecmp (key, "partial_recovery", 17))
    {
        if (IS_FAILURE (set_bool_value (&restore_opt->partial_recovery, value)))
        {
            goto error;
        }
    }
    else if (0 == strncasecmp (key, "use_database_location_path", 27))
    {
        if (IS_FAILURE (set_bool_value (&restore_opt->use_database_location_path, value)))
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
int compile_regex (regex_t* re_opt_header, regex_t* re_opt, regex_t* re_empty_line)
{
    char* regex_header     = "^[[:blank:]]*\\[[[:blank:]]*(backup|restore)[[:blank:]]*\\][[:space:]]*$";
    char* regex_key_value  = "^[[:blank:]]*([[:alpha:]]+)[[:blank:]]*=[[:blank:]]*([[:alnum:]]+)[[:space:]]*$";
    char* regex_empty_line = "^[[:space:]]*$";

    if (IS_FAILURE (regcomp (re_opt_header, regex_header, REG_ICASE | REG_EXTENDED)))
    {
        goto error;
    }

    if (IS_FAILURE (regcomp (re_opt, regex_key_value, REG_ICASE | REG_EXTENDED)))
    {
        goto error;
    }

    if (IS_FAILURE (regcomp (re_empty_line, regex_empty_line, REG_ICASE | REG_EXTENDED)))
    {
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
int free_regex (regex_t* re_opt_header, regex_t* re_opt, regex_t* re_empty_line)
{
    regfree (re_opt_header);
    regfree (re_opt);
    regfree (re_empty_line);

    return SUCCESS;
}

static
int read_option (FILE* conf_fp)
{
    regex_t re_opt_header;
    regex_t re_opt;
    regex_t re_empty_line;

    regmatch_t match[3];

    char line_str[1024];

    char* key;
    char* value;

    enum read_state
    {
        BACKUP_OPTION_READ_STATE,
        RESTORE_OPTION_READ_STATE,
        NO_READ_STATE
    };

    enum read_state read_state = NO_READ_STATE;

    int state = 0;

    if (IS_FAILURE (compile_regex (&re_opt_header, &re_opt, &re_empty_line)))
    {
        goto error;
    }

    state = 1;

    while (NULL != fgets (line_str, 1024, conf_fp))
    {
        /* empty string */
        if (IS_SUCCESS (regexec (&re_empty_line, line_str, 3, match, 0)))
        {
            continue;
        }

        /* [header] */
        if (IS_SUCCESS (regexec (&re_opt_header, line_str, 3, match, 0)))
        {
            /* [backup] */
            if (line_str[match[1].rm_so] == 'b')
            {
                read_state = BACKUP_OPTION_READ_STATE;

                continue;
            }
            /* [restore] */
            else if (line_str[match[1].rm_so] == 'r')
            {
                read_state = RESTORE_OPTION_READ_STATE;

                continue;
            }
        }

        /* key = value */
        if (IS_SUCCESS (regexec (&re_opt, line_str, 3, match, 0)))
        {
            key = &line_str[match[1].rm_so];
            line_str[match[1].rm_eo] = '\0';

            value = &line_str[match[2].rm_so];
            line_str[match[2].rm_eo] = '\0';

            if (read_state == BACKUP_OPTION_READ_STATE)
            {
                if (IS_FAILURE (set_backup_option (key, value)))
                {
                    goto error;
                }
            }
            else if (read_state == RESTORE_OPTION_READ_STATE)
            {
                if (IS_FAILURE (set_restore_option (key, value)))
                {
                    goto error;
                }
            }
            else
            {
                goto error;
            }
        }
    }

    free_regex (&re_opt_header, &re_opt, &re_empty_line);

    return SUCCESS;

error:

    switch (state)
    {
        case 1:
            free_regex (&re_opt_header, &re_opt, &re_empty_line);
        default:
            break;
    }

    return FAILURE;
}

static
int read_conf_file (void)
{
    char conf_file[PATH_MAX];
    FILE* conf_fp = NULL;

    int state = 0;

    init_default_backup_option ();
    init_default_restore_option ();

    snprintf (conf_file, PATH_MAX, "%s/conf/cubrid_backup.conf", backup_mgr->cubrid_home);

    if (IS_FAILURE (check_path_length_limit (conf_file)))
    {
        goto end;
    }

    conf_fp = fopen (conf_file, "r");
    if (conf_fp == NULL)
    {
        goto end;
    }

    state = 1;

    if (IS_FAILURE (read_option (conf_fp)))
    {
        goto error;
    }

    fclose (conf_fp);

end:

    return SUCCESS;

error:

    switch (state)
    {
        case 1:
            fclose (conf_fp);
        default:
            break;
    }

    return FAILURE;
}

static
int set_io_size (void)
{
    struct stat backup_home_stat;

    if (IS_FAILURE (stat (backup_mgr->backup_home, &backup_home_stat)))
    {
        goto error;
    }

    backup_mgr->io_size = backup_home_stat.st_blksize * 8;

    return SUCCESS;

error:

    return FAILURE;
}

int start_backup_manager (void)
{
    if (IS_FAILURE (open_log_file ()))
    {
        goto error;
    }

    if (IS_FAILURE (set_cubrid_home ()))
    {
        goto error;
    }

    if (IS_FAILURE (make_backup_home ()))
    {
        goto error;
    }

    if (IS_FAILURE (read_conf_file ()))
    {
        goto error;
    }

    if (IS_FAILURE (set_io_size ()))
    {
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

int stop_backup_manager (void)
{
    if (IS_FAILURE (remove_backup_home ()))
    {
        goto error;
    }

    close_log_file ();

    return SUCCESS;

error:

    return FAILURE;
}
