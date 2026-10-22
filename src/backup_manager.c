#include <stdlib.h>
#include <stdarg.h>
#include <regex.h>
#include <time.h>
#include <sys/timeb.h>
#include <errno.h>
#include <ctype.h>
#include <locale.h>
#include <assert.h>
#include <pthread.h>
#include <sys/statvfs.h>
#include "backup_manager.h"

#define INT_MAX 2147483647

#ifndef LLONG_MAX
#define LLONG_MAX 9223372036854775807LL
#endif

#define FIFO_SIZE_MIN         (64 * 1024)   /* default pipe capacity */
#define FIFO_SIZE_DEFAULT_MAX (1024 * 1024) /* default pipe-max-size */

#define LOG_HEADER_MAX_SIZE  (50)
#define LOG_MESSAGE_MAX_SIZE (LOG_HEADER_MAX_SIZE + 1024)

BACKUP_MANAGER backup_manager;

BACKUP_MANAGER* backup_mgr = &backup_manager;

/* print_log() serializes on this leaf lock: once the drain thread also logs,
 * the static log_buffer and the append to log_fp are shared state. */
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

/* fork-safety: the backup child forked in execute_backup runs briefly before
 * execv and may call print_log, which takes log_mutex. If another thread (e.g. the
 * drain) held log_mutex at the instant of fork, the child would inherit it locked
 * and self-deadlock. These pthread_atfork handlers acquire it around fork so the
 * child inherits a consistent mutex and then unlocks it. */
static void log_atfork_prepare (void) { pthread_mutex_lock (&log_mutex); }
static void log_atfork_parent  (void) { pthread_mutex_unlock (&log_mutex); }
static void log_atfork_child   (void) { pthread_mutex_unlock (&log_mutex); }

static
int make_log_header (char* buf, size_t buf_len, const char *prefix_str)
{
    struct tm tm, * tm_p;
    time_t sec;
    int millisec, len;
    struct timeb tb;
    char* p;

    if (buf == NULL)
    {
        return 0;
    }

    /* current time */
    (void) ftime (&tb);
    sec = tb.time;
    millisec = tb.millitm;

    tm_p = localtime_r (&sec, &tm);

    len = (int) strftime (buf, buf_len, "%y-%m-%d %H:%M:%S", tm_p);
    p = buf + len;
    buf_len -= len;

    len += snprintf (p, buf_len, ".%03d (%d) %s ", millisec, getpid (), (prefix_str ? prefix_str : ""));

    assert (len <= LOG_HEADER_MAX_SIZE);

    return len;
}

int print_log (const char *prefix_str, const char *msg, ...)
{
    static char log_buffer[LOG_MESSAGE_MAX_SIZE];

    char* p;
    int len, n;

    va_list arg_list;

    if (IS_NULL (backup_mgr->log_fp))
    {
        return SUCCESS;
    }

    pthread_mutex_lock (&log_mutex);

    p = log_buffer;
    len = LOG_MESSAGE_MAX_SIZE;
    n = make_log_header (p, len, prefix_str);
    len -= n;
    p += n;

    if (len > 0)
    {
        va_start (arg_list, msg);
        n = vsnprintf (p, len, msg, arg_list);
        if (n >= len)
        {
            p[len - 1] = '\0';
        }
        va_end (arg_list);
    }

    /* "%s": log_buffer is data, not a format string — conf-supplied values
     * (e.g. buffer_disk_path) may contain '%'. */
    fprintf (backup_mgr->log_fp, "%s", log_buffer);
    fflush (backup_mgr->log_fp);

    pthread_mutex_unlock (&log_mutex);

    return SUCCESS;
}

static
int open_log_file (void)
{
    char log_file_path[PATH_MAX];

    const char *cubrid_env = getenv ("CUBRID");
    if (cubrid_env == NULL)
    {
        goto error;
    }

    snprintf (log_file_path, PATH_MAX, "%s/log/cubrid_backup.log", cubrid_env);

    backup_mgr->log_fp = fopen (log_file_path, "a");
    if (backup_mgr->log_fp == NULL)
    {
        //PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    return SUCCESS;

error:

    return FAILURE;
}

static
int close_log_file (void)
{
    if (backup_mgr->log_fp != NULL)
    {
        fclose (backup_mgr->log_fp);
    }

    backup_mgr->log_fp = NULL;

    return SUCCESS;
}

int validate_dir (const char* path)
{
    struct stat path_stat;

    if (IS_NULL (path) || strlen (path) >= PATH_MAX)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error; 
    }

    if (IS_FAILURE (access (path, R_OK | W_OK | X_OK)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (stat (path, &path_stat)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }
    else
    {
        if (!S_ISDIR (path_stat.st_mode))
        {
            PRINT_LOG_ERR (ERR_INFO);
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
        PRINT_LOG_ERR (ERR_INFO);
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
        PRINT_LOG_ERR (ERR_INFO);
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
        PRINT_LOG_ERR (ERR_INFO);
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

    // A check here may emit an error log, but that is not a bug.
    if (IS_SUCCESS (validate_dir (backup_path)))
    {

    }
    else
    {
        /* (2) $CUBRID_TMP */
        const char *cubrid_tmp = getenv ("CUBRID_TMP");

        /* an empty CUBRID_TMP is treated as unset, as before */
        if (cubrid_tmp != NULL && cubrid_tmp[0] == '\0')
        {
            cubrid_tmp = NULL;
        }

        if (cubrid_tmp != NULL)
        {
            /* a relative CUBRID_TMP would resolve against the caller's cwd */
            if (cubrid_tmp[0] != '/')
            {
                PRINT_LOG_ERR ("CUBRID_TMP must be an absolute path: '%s'\n", cubrid_tmp);
                goto error;
            }

            snprintf (backup_path, PATH_MAX, "%s", cubrid_tmp);
            remove_trailing_slash (backup_path);
        }

        // A check here may emit an error log, but that is not a bug.
        if (cubrid_tmp != NULL && IS_SUCCESS (validate_dir (backup_path)))
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
                PRINT_LOG_ERR (ERR_INFO);
                goto error;
            }
        }
    }

    snprintf (backup_mgr->backup_home, PATH_MAX, "%s/%s", backup_path, ".cubrid_backup");

    if (IS_FAILURE (check_path_length_limit (backup_mgr->backup_home)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    PRINT_LOG_INFO ("backup working directory: %s\n", backup_mgr->backup_home);

    if (IS_FAILURE (mkdir (backup_mgr->backup_home, S_IRWXU)))
    {
        if (errno != EEXIST)
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
        else
        {
            if (IS_FAILURE (validate_dir (backup_mgr->backup_home)))
            {
                PRINT_LOG_ERR (ERR_INFO);
                goto error;
            }
            else
            {
                if (IS_FAILURE (remove_all_entries (backup_mgr->backup_home)))
                {
                    PRINT_LOG_ERR (ERR_INFO);
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
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

#if 0
    if (IS_FAILURE (rmdir (backup_mgr->backup_home)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }
#endif

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

    backup_opt->fifo_size              = FIFO_SIZE_MIN;      /* 64KB               */
    backup_opt->buffer_memory_size     = 0;                  /* buffering OFF (opt-in): set >0 to enable the drain/tiered-buffer path */
    backup_opt->buffer_disk_limit      = 0;                  /* disk tier off      */
    backup_opt->buffer_disk_path[0]    = '\0';
    backup_opt->buffer_disk_keep_spool = false;

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

    for (i = 0; i < strlen (src); i++)
    {
        if (src[i] < '0' || src[i] > '9')
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }

    errno = 0;
    long_val = strtol (src, NULL, 10);

    if (errno == ERANGE || long_val > INT_MAX)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    *dest = (int) long_val;

    return SUCCESS;

error:

    return FAILURE;
}

/* Size value: [<digits>][.<digits>][unit] with at least one digit, unit one
 * of B, K/KB, M/MB, G/GB, T/TB, P/PB (case-insensitive, 1024-based), e.g.
 * 64KB, 1.5G, .5G, 1.G. Mirrors the units and decimal forms CUBRID accepts
 * for its own size parameters; a sign, hex, exponent, inf/nan (which CUBRID's
 * strtod would also take), a missing number or an overflow is rejected.
 * A fractional byte count is truncated. Integers stay exact (CUBRID rounds
 * through a double above 2^53). */
static
int set_size_value (long long* dest, char* src)
{
    static const struct
    {
        const char* unit;
        long long factor;
    } units[] =
    {
        { "B",  1LL },
        { "K",  1LL << 10 }, { "KB", 1LL << 10 },
        { "M",  1LL << 20 }, { "MB", 1LL << 20 },
        { "G",  1LL << 30 }, { "GB", 1LL << 30 },
        { "T",  1LL << 40 }, { "TB", 1LL << 40 },
        { "P",  1LL << 50 }, { "PB", 1LL << 50 },
    };

    char* p = src;
    char* frac_start = NULL;
    long long int_part = 0;
    long long factor = 1;
    int n_digits = 0;
    int i;

    for (; isdigit ((unsigned char) *p); p++, n_digits++)
    {
        if (int_part > (LLONG_MAX - (*p - '0')) / 10)
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        int_part = int_part * 10 + (*p - '0');
    }

    if (*p == '.')
    {
        frac_start = p;
        p++;

        while (isdigit ((unsigned char) *p))
        {
            p++;
            n_digits++;
        }
    }

    /* either side of the point may be empty (.5G, 1.G), but not both */
    if (n_digits == 0)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (*p != '\0')
    {
        for (i = 0; i < (int) (sizeof (units) / sizeof (units[0])); i++)
        {
            if (IS_ZERO (strcasecmp (p, units[i].unit)))
            {
                factor = units[i].factor;
                break;
            }
        }

        if (i == (int) (sizeof (units) / sizeof (units[0])))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }

    if (frac_start != NULL)
    {
        /* Same arithmetic as CUBRID (util_size_string_to_byte): strtod of the
         * number times the unit factor, truncated. The "C" locale keeps '.' as
         * the decimal point whatever the host's LC_NUMERIC is. */
        char num_str[1024];   /* a value cannot exceed one conf line */
        size_t num_len = (size_t) (p - src);
        locale_t c_locale;
        double val;

        if (num_len >= sizeof (num_str))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        memcpy (num_str, src, num_len);
        num_str[num_len] = '\0';

        c_locale = newlocale (LC_NUMERIC_MASK, "C", (locale_t) 0);
        if (c_locale == (locale_t) 0)
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        val = strtod_l (num_str, NULL, c_locale) * (double) factor;
        freelocale (c_locale);

        if (!(val < 9223372036854775808.0))   /* 2^63: overflow guard */
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        *dest = (long long) val;

        return SUCCESS;
    }

    if (int_part > LLONG_MAX / factor)   /* unit-multiply overflow guard */
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    *dest = int_part * factor;

    return SUCCESS;

error:

    return FAILURE;
}

static
int set_path_value (char* dest, size_t dest_size, char* src)
{
    if (strlen (src) >= dest_size)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    snprintf (dest, dest_size, "%s", src);

    return SUCCESS;

error:

    return FAILURE;
}

static
int set_backup_option (char* key, char* value)
{
    BACKUP_OPTION* backup_opt;

    long long sz;

    backup_opt = &backup_mgr->default_backup_option;

    if (IS_ZERO (strncasecmp (key, "remove_archive", 15)))
    {
        if (IS_FAILURE (set_bool_value (&backup_opt->remove_archive, value)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }
    else if (IS_ZERO (strncasecmp (key, "sa_mode", 8)))
    {
        if (IS_FAILURE (set_bool_value (&backup_opt->sa_mode, value)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }
    else if (IS_ZERO (strncasecmp (key, "no_check", 9)))
    {
        if (IS_FAILURE (set_bool_value (&backup_opt->no_check, value)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }
    else if (IS_ZERO (strncasecmp (key, "thread_count", 13)))
    {
        if (IS_FAILURE (set_int_value (&backup_opt->thread_count, value)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }
    else if (IS_ZERO (strncasecmp (key, "compress", 9)))
    {
        if (IS_FAILURE (set_bool_value (&backup_opt->compress, value)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }
    else if (IS_ZERO (strncasecmp (key, "except_active_log", 18)))
    {
        if (IS_FAILURE (set_bool_value (&backup_opt->except_active_log, value)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }
    else if (IS_ZERO (strncasecmp (key, "sleep_msecs", 12)))
    {
        if (IS_FAILURE (set_int_value (&backup_opt->sleep_msecs, value)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }
    else if (IS_ZERO (strncasecmp (key, "fifo_size", 10)))
    {
        if (IS_FAILURE (set_size_value (&sz, value)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        /* fifo_size is an int (as is the F_SETPIPE_SZ argument): out of type range, not clamped */
        if (sz > INT_MAX)
        {
            PRINT_LOG_ERR ("cubrid_backup.conf: fifo_size=%s exceeds the int range (max %d)\n", value, INT_MAX);
            goto error;
        }

        backup_opt->fifo_size = (int) sz;
    }
    else if (IS_ZERO (strncasecmp (key, "buffer_memory_size", 19)))
    {
        if (IS_FAILURE (set_size_value (&backup_opt->buffer_memory_size, value)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }
    else if (IS_ZERO (strncasecmp (key, "buffer_disk_limit", 18)))
    {
        if (IS_FAILURE (set_size_value (&backup_opt->buffer_disk_limit, value)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }
    else if (IS_ZERO (strncasecmp (key, "buffer_disk_path", 17)))
    {
        if (IS_FAILURE (set_path_value (backup_opt->buffer_disk_path, PATH_MAX, value)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }
    else if (IS_ZERO (strncasecmp (key, "buffer_disk_keep_spool", 23)))
    {
        if (IS_FAILURE (set_bool_value (&backup_opt->buffer_disk_keep_spool, value)))
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
int set_restore_option (char* key, char* value)
{
    RESTORE_OPTION* restore_opt;

    restore_opt = &backup_mgr->default_restore_option;

    if (0 == strncasecmp (key, "partial_recovery", 17))
    {
        if (IS_FAILURE (set_bool_value (&restore_opt->partial_recovery, value)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }
    }
    else if (0 == strncasecmp (key, "use_database_location_path", 27))
    {
        if (IS_FAILURE (set_bool_value (&restore_opt->use_database_location_path, value)))
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
int compile_regex (regex_t* re_opt_header, regex_t* re_opt, regex_t* re_empty_line)
{
    /* any [name]; the section name is classified by the caller */
    char* regex_header     = "^[[:blank:]]*\\[[[:blank:]]*([^][:blank:]]+)[[:blank:]]*\\][[:space:]]*$";
    char* regex_key_value  = "^[[:blank:]]*([_[:alpha:]]+)[[:blank:]]*=[[:blank:]]*([[:alnum:]/._-]+)[[:space:]]*$";
    char* regex_empty_line = "^[[:space:]]*$";

    if (IS_FAILURE (regcomp (re_opt_header, regex_header, REG_ICASE | REG_EXTENDED)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (regcomp (re_opt, regex_key_value, REG_ICASE | REG_EXTENDED)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (regcomp (re_empty_line, regex_empty_line, REG_ICASE | REG_EXTENDED)))
    {
        PRINT_LOG_ERR (ERR_INFO);
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
    int line_no = 0;
    long line_pos;

    char* key;
    char* value;

    enum read_state
    {
        BACKUP_OPTION_READ_STATE,
        RESTORE_OPTION_READ_STATE,
        UNKNOWN_SECTION_READ_STATE,
        NO_READ_STATE
    };

    enum read_state read_state = NO_READ_STATE;

    int state = 0;

    if (IS_FAILURE (compile_regex (&re_opt_header, &re_opt, &re_empty_line)))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    state = 1;

    for (line_pos = ftell (conf_fp);
         NULL != fgets (line_str, sizeof (line_str), conf_fp);
         line_pos = ftell (conf_fp))
    {
        size_t line_len = strlen (line_str);
        long next_pos = ftell (conf_fp);

        line_no++;

        /* fgets does not stop at a NUL byte, so strlen would hide the rest of
         * the chunk (and defeat the length check below); reject such lines. */
        if (line_pos >= 0 && next_pos >= 0 && (size_t) (next_pos - line_pos) != line_len)
        {
            PRINT_LOG_ERR ("cubrid_backup.conf:%d: line contains a NUL byte\n", line_no);
            goto error;
        }

        /* A chunk without '\n' is either the unterminated last line or a line
         * longer than the buffer; the latter is rejected instead of letting
         * its tail be parsed as the next line. */
        if (line_len == sizeof (line_str) - 1 && line_str[line_len - 1] != '\n')
        {
            int c = getc (conf_fp);

            if (c == '\r')
            {
                c = getc (conf_fp);   /* CRLF terminator: '\r' is not line content */
            }

            if (c != EOF && c != '\n')
            {
                PRINT_LOG_ERR ("cubrid_backup.conf:%d: line longer than %d characters\n",
                               line_no, (int) sizeof (line_str) - 1);
                goto error;
            }
        }

        /* empty string */
        if (IS_SUCCESS (regexec (&re_empty_line, line_str, 3, match, 0)))
        {
            continue;
        }

        /* [header] */
        if (IS_SUCCESS (regexec (&re_opt_header, line_str, 3, match, 0)))
        {
            char* section = &line_str[match[1].rm_so];
            line_str[match[1].rm_eo] = '\0';

            if (IS_ZERO (strcasecmp (section, "backup")))
            {
                read_state = BACKUP_OPTION_READ_STATE;
            }
            else if (IS_ZERO (strcasecmp (section, "restore")))
            {
                read_state = RESTORE_OPTION_READ_STATE;
            }
            else
            {
                /* keys up to the next known header belong to this section */
                read_state = UNKNOWN_SECTION_READ_STATE;

                PRINT_LOG_WARN ("cubrid_backup.conf:%d: unknown section [%s]; ignoring its keys\n",
                                line_no, section);
            }

            continue;
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
                    PRINT_LOG_ERR ("cubrid_backup.conf:%d: invalid [backup] option: %s=%s\n",
                                   line_no, key, value);
                    goto error;
                }
            }
            else if (read_state == RESTORE_OPTION_READ_STATE)
            {
                if (IS_FAILURE (set_restore_option (key, value)))
                {
                    PRINT_LOG_ERR ("cubrid_backup.conf:%d: invalid [restore] option: %s=%s\n",
                                   line_no, key, value);
                    goto error;
                }
            }
            else if (read_state == UNKNOWN_SECTION_READ_STATE)
            {
                continue;
            }
            else
            {
                PRINT_LOG_ERR ("cubrid_backup.conf:%d: option outside of any section: %s=%s\n",
                               line_no, key, value);
                goto error;
            }
        }
    }

    if (ferror (conf_fp))
    {
        PRINT_LOG_ERR ("cubrid_backup.conf: read error after line %d: %s\n", line_no, strerror (errno));
        goto error;
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

    if (snprintf (conf_file, PATH_MAX, "%s/conf/cubrid_backup.conf", backup_mgr->cubrid_home) >= PATH_MAX ||
        IS_FAILURE (check_path_length_limit (conf_file)))
    {
        PRINT_LOG_ERR ("cubrid_backup.conf path is too long (CUBRID=%s)\n", backup_mgr->cubrid_home);
        goto error;
    }

    conf_fp = fopen (conf_file, "r");
    if (conf_fp == NULL)
    {
        /* no conf file: all defaults */
        if (errno == ENOENT)
        {
            goto end;
        }

        PRINT_LOG_ERR ("cannot open %s: %s\n", conf_file, strerror (errno));
        goto error;
    }

    state = 1;

    if (IS_FAILURE (read_option (conf_fp)))
    {
        PRINT_LOG_ERR (ERR_INFO);
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
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    backup_mgr->io_size = backup_home_stat.st_blksize * 8;

    return SUCCESS;

error:

    return FAILURE;
}

/* Linux pipe capacity limit for unprivileged F_SETPIPE_SZ; 1MB (the kernel
 * default) when /proc/sys/fs/pipe-max-size cannot be read. */
static
int read_pipe_max_size (void)
{
    FILE* fp;
    long val = 0;

    fp = fopen ("/proc/sys/fs/pipe-max-size", "r");
    if (fp != NULL)
    {
        if (fscanf (fp, "%ld", &val) != 1)
        {
            val = 0;
        }

        fclose (fp);
    }

    if (val <= 0)
    {
        PRINT_LOG_WARN ("cannot read /proc/sys/fs/pipe-max-size; assuming %d\n", FIFO_SIZE_DEFAULT_MAX);
        return FIFO_SIZE_DEFAULT_MAX;
    }

    /* the kernel allows up to 2^31; fifo_size is an int */
    return (val > INT_MAX) ? INT_MAX : (int) val;
}

static
int validate_backup_option (void)
{
    BACKUP_OPTION* backup_opt;
    long long min_disk_limit;
    int pipe_max;
    int fifo_max;

    backup_opt = &backup_mgr->default_backup_option;

    /* fifo_size: below 64KB is a configuration error; above the OS pipe limit
     * (pipe-max-size) it is lowered to that limit. */
    if (backup_opt->fifo_size < FIFO_SIZE_MIN)
    {
        PRINT_LOG_ERR ("cubrid_backup.conf: fifo_size=%d is below the minimum %d (64KB)\n",
                       backup_opt->fifo_size, FIFO_SIZE_MIN);
        goto error;
    }

    pipe_max = read_pipe_max_size ();

    /* never lower fifo_size below the 64KB minimum, even if pipe-max-size is
     * configured smaller */
    fifo_max = (pipe_max < FIFO_SIZE_MIN) ? FIFO_SIZE_MIN : pipe_max;

    if (backup_opt->fifo_size > fifo_max)
    {
        PRINT_LOG_WARN ("fifo_size=%d exceeds pipe-max-size=%d; clamping to %d\n",
                        backup_opt->fifo_size, pipe_max, fifo_max);
        backup_opt->fifo_size = fifo_max;
    }

    /* memory tier must hold at least one io_size chunk when enabled */
    if (backup_opt->buffer_memory_size != 0 &&
        backup_opt->buffer_memory_size < backup_mgr->io_size)
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    /* disk tier is an overflow of the memory tier and needs a valid spool dir */
    if (backup_opt->buffer_disk_limit > 0)
    {
        struct statvfs vfs;

        if (backup_opt->buffer_memory_size == 0)
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        /* a relative spool dir would resolve against the caller's cwd; the
         * path is unused (and not checked) when the disk tier is off */
        if (backup_opt->buffer_disk_path[0] != '\0' && backup_opt->buffer_disk_path[0] != '/')
        {
            PRINT_LOG_ERR ("cubrid_backup.conf: buffer_disk_path must be an absolute path: '%s'\n",
                           backup_opt->buffer_disk_path);
            goto error;
        }

        if (IS_FAILURE (validate_dir (backup_opt->buffer_disk_path)))
        {
            PRINT_LOG_ERR (ERR_INFO);
            goto error;
        }

        /* the spool must hold at least one pipe-full / one io_size chunk */
        min_disk_limit = (backup_opt->fifo_size > backup_mgr->io_size)
                       ? backup_opt->fifo_size : backup_mgr->io_size;

        if (backup_opt->buffer_disk_limit < min_disk_limit)
        {
            PRINT_LOG_ERR ("cubrid_backup.conf: buffer_disk_limit=%lld is below max(fifo_size, io_size)=%lld\n",
                           backup_opt->buffer_disk_limit, min_disk_limit);
            goto error;
        }

        /* free-space shortfall is a warning, not a failure (an unreservable spool fails cubrid_backup_begin) */
        if (statvfs (backup_opt->buffer_disk_path, &vfs) == 0)
        {
            long long avail = (long long) vfs.f_bavail * (long long) vfs.f_frsize;

            if (avail < backup_opt->buffer_disk_limit)
            {
                PRINT_LOG_WARN ("spool free space is below buffer_disk_limit\n");
            }
        }
    }

    return SUCCESS;

error:

    return FAILURE;
}

int start_backup_manager (void)
{
    static bool atfork_registered = false;

    if (!atfork_registered)                 /* register once per process, before any backup fork */
    {
        (void) pthread_atfork (log_atfork_prepare, log_atfork_parent, log_atfork_child);
        atfork_registered = true;
    }

    if (IS_FAILURE (open_log_file ()))
    {
        //PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (set_cubrid_home ()))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (make_backup_home ()))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (read_conf_file ()))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (set_io_size ()))
    {
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    if (IS_FAILURE (validate_backup_option ()))
    {
        PRINT_LOG_ERR (ERR_INFO);
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
        PRINT_LOG_ERR (ERR_INFO);
        goto error;
    }

    close_log_file ();

    return SUCCESS;

error:

    return FAILURE;
}
