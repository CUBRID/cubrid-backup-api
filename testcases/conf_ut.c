/*
 * conf_ut - deterministic unit tests for cubrid_backup.conf parsing and
 * validation (start_backup_manager -> read_conf_file -> validate_backup_option).
 * No CUBRID server or database is needed.
 *
 * Includes backup_manager.c directly to reach its static parsers
 * (set_size_value / set_int_value). Each case writes a conf into a private
 * $CUBRID under $TMPDIR (default /tmp), runs start_backup_manager() and checks
 * the return code, the parsed values and the log lines.
 *
 * Covers APIS-1102 PART A: size units/fractions, fifo_size range,
 * buffer_disk_limit >= max(pipe capacity for fifo_size, io_size), absolute buffer_disk_path,
 * unreadable conf, too-long conf path, unknown/upper-case sections, over-long
 * lines, relative CUBRID_TMP, errno reset in set_int_value, and the conf blocks
 * used by testcases/cubrid_backup.conf, run_test.sh and the stress scripts.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../src/backup_manager.c"

static int g_fail = 0, g_checks = 0;
#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fail++; printf ("  [NOK] %s\n", msg); } \
    else printf ("  [OK] %s\n", msg); } while (0)

#define KB (1024LL)
#define MB (1024LL * 1024)
#define GB (1024LL * 1024 * 1024)
#define TB (GB * 1024)
#define PB (TB * 1024)

static char g_home[PATH_MAX];       /* the private $CUBRID */
static char g_spool[PATH_MAX];      /* an existing absolute spool dir */
static char g_conf[PATH_MAX];
static char g_log[PATH_MAX];
static int  g_pipe_max;

static BACKUP_OPTION*  bo (void) { return &backup_mgr->default_backup_option; }
static RESTORE_OPTION* ro (void) { return &backup_mgr->default_restore_option; }

static void write_file (const char* path, const char* text)
{
    FILE* fp = fopen (path, "w");
    assert (fp != NULL);
    fputs (text, fp);
    fclose (fp);
}

/* Replace "@SPOOL@" with the absolute spool dir and write the conf. */
static void write_conf (const char* text)
{
    char buf[8192];
    const char* s = text;
    char* d = buf;
    const char* hit;

    while ((hit = strstr (s, "@SPOOL@")) != NULL)
    {
        memcpy (d, s, (size_t) (hit - s));
        d += hit - s;
        d += sprintf (d, "%s", g_spool);
        s = hit + strlen ("@SPOOL@");
    }
    strcpy (d, s);

    write_file (g_conf, buf);
}

/* One init attempt: truncate the log, start, then stop (closes the log). */
static int run_init (void)
{
    int rc;

    write_file (g_log, "");
    rc = start_backup_manager ();
    stop_backup_manager ();

    return rc;
}

static int run_conf (const char* text)
{
    if (text == NULL)
    {
        unlink (g_conf);
    }
    else
    {
        write_conf (text);
    }

    return run_init ();
}

static int log_has (const char* needle)
{
    static char buf[65536];
    FILE* fp = fopen (g_log, "r");
    size_t n;

    if (fp == NULL)
    {
        return 0;
    }
    n = fread (buf, 1, sizeof (buf) - 1, fp);
    buf[n] = '\0';
    fclose (fp);

    return strstr (buf, needle) != NULL;
}

/* ── 1. set_size_value / set_int_value (table-driven) ── */

static void t_size_parser (void)
{
    static const struct { const char* in; int ok; long long want; } cases[] =
    {
        { "0",        1, 0 },
        { "0KB",      1, 0 },
        { "512",      1, 512 },
        { "512B",     1, 512 },
        { "512b",     1, 512 },
        { "64K",      1, 64 * KB },
        { "64KB",     1, 64 * KB },
        { "64kb",     1, 64 * KB },
        { "64Kb",     1, 64 * KB },
        { "8M",       1, 8 * MB },
        { "8MB",      1, 8 * MB },
        { "2G",       1, 2 * GB },
        { "2GB",      1, 2 * GB },
        { "3T",       1, 3 * TB },
        { "3TB",      1, 3 * TB },
        { "1P",       1, PB },
        { "1PB",      1, PB },
        { "1.5G",     1, 3 * GB / 2 },
        { "1.5GB",    1, 3 * GB / 2 },
        { "0.5K",     1, 512 },
        { "1.25M",    1, 5 * MB / 4 },
        { "1.5",      1, 1 },                   /* fractional byte truncated */
        { "1.0009765625K", 1, 1025 },
        { "0.015625K", 1, 16 },                 /* same arithmetic as CUBRID: strtod * factor */
        { "63.015625K", 1, 64528 },
        { "1.015625G", 1, 1090519040 },
        { ".5G",      1, GB / 2 },              /* empty integer part (CUBRID strtod) */
        { "1.G",      1, GB },                  /* empty fraction (CUBRID strtod) */
        { "1.",       1, 1 },
        { "9223372036854775807", 1, 9223372036854775807LL },
        { "8191P",    1, 8191 * PB },
        { "8191.999999999999P", 1, -1 },        /* fits: < 2^63; value checked separately */
        /* rejected */
        { "MB",       0, 0 },
        { "GB",       0, 0 },
        { "KB",       0, 0 },
        { "B",        0, 0 },
        { "",         0, 0 },
        { "-1",       0, 0 },
        { "+1",       0, 0 },
        { "-5MB",     0, 0 },
        { ".",        0, 0 },
        { ".G",       0, 0 },
        { ".KB",      0, 0 },
        { "1.5.5M",   0, 0 },
        { "0x10",     0, 0 },
        { "1e3",      0, 0 },
        { "1E3K",     0, 0 },
        { "inf",      0, 0 },
        { "nan",      0, 0 },
        { "5QB",      0, 0 },
        { "1KBB",     0, 0 },
        { "1 KB",     0, 0 },
        { "1KiB",     0, 0 },
        { "abc",      0, 0 },
        { "9223372036854775808", 0, 0 },
        { "99999999999999999999999", 0, 0 },
        { "8192P",    0, 0 },
        { "9000000000000G", 0, 0 },
    };
    char msg[256];
    size_t i;

    printf ("== set_size_value\n");
    for (i = 0; i < sizeof (cases) / sizeof (cases[0]); i++)
    {
        long long v = -7;
        char in[64];
        int rc;

        snprintf (in, sizeof (in), "%s", cases[i].in);
        rc = set_size_value (&v, in);

        if (cases[i].ok)
        {
            snprintf (msg, sizeof (msg), "size '%s' accepted = %lld", cases[i].in, cases[i].want);
            CHECK (rc == SUCCESS && (cases[i].want < 0 ? v > 8191 * PB : v == cases[i].want), msg);
        }
        else
        {
            snprintf (msg, sizeof (msg), "size '%s' rejected (dest untouched)", cases[i].in);
            CHECK (rc == FAILURE && v == -7, msg);
        }
    }
}

static void t_int_parser (void)
{
    int v;
    char s1[] = "5", s2[] = "2147483647", s3[] = "2147483648", s4[] = "99999999999999999999",
         s5[] = "-1", s6[] = "1K", s7[] = "0";

    printf ("== set_int_value\n");

    errno = ERANGE;   /* stale errno from an earlier call must not fail a valid value */
    v = -1;
    CHECK (set_int_value (&v, s1) == SUCCESS && v == 5, "int '5' accepted with stale errno=ERANGE");
    CHECK (set_int_value (&v, s2) == SUCCESS && v == 2147483647, "int INT_MAX accepted");
    CHECK (set_int_value (&v, s3) == FAILURE, "int INT_MAX+1 rejected");
    CHECK (set_int_value (&v, s4) == FAILURE, "int ERANGE rejected");
    CHECK (set_int_value (&v, s5) == FAILURE, "int '-1' rejected (digits only)");
    CHECK (set_int_value (&v, s6) == FAILURE, "int '1K' rejected (integer keys take no unit)");
    CHECK (set_int_value (&v, s7) == SUCCESS && v == 0, "int '0' accepted");
}

/* ── 2. end-to-end init cases (table-driven) ── */

struct init_case
{
    const char* name;
    const char* conf;     /* NULL = no conf file */
    int         ok;       /* expected start_backup_manager() success */
    const char* log_needle;
};

static const struct init_case init_cases[] =
{
    /* item 1: unit without a number */
    { "1 buffer_memory_size=MB fails",
      "[backup]\nbuffer_memory_size=MB\n", 0, "cubrid_backup.conf:2: invalid [backup] option: buffer_memory_size=MB" },
    { "1 buffer_disk_limit=GB fails",
      "[backup]\nbuffer_memory_size=1MB\nbuffer_disk_limit=GB\nbuffer_disk_path=@SPOOL@\n", 0, "buffer_disk_limit=GB" },
    { "1 fifo_size=KB fails",
      "[backup]\nfifo_size=KB\n", 0, "fifo_size=KB" },
    { "1 thread_count=1K fails (integer key)",
      "[backup]\nthread_count=1K\n", 0, "thread_count=1K" },
    { "1 buffer_memory_size=0x10 fails",
      "[backup]\nbuffer_memory_size=0x10\n", 0, "buffer_memory_size=0x10" },

    /* item 2: fifo_size lower bound */
    { "2 fifo_size=10 fails",
      "[backup]\nfifo_size=10\n", 0, "fifo_size=10 is below the minimum" },
    { "2 fifo_size=0 fails",
      "[backup]\nfifo_size=0\n", 0, "fifo_size=0 is below the minimum" },
    { "2 fifo_size=65535 fails",
      "[backup]\nfifo_size=65535\n", 0, "below the minimum" },
    { "2 fifo_size=1KB fails (s01 A0)",
      "[backup]\nbuffer_memory_size=1MB\nbuffer_disk_limit=256MB\nbuffer_disk_path=@SPOOL@\nfifo_size=1KB\n", 0, "below the minimum" },
    { "2 fifo_size=3GB fails (> INT_MAX)",
      "[backup]\nfifo_size=3GB\n", 0, "fifo_size=3GB" },
    { "2 fifo_size=64KB ok",
      "[backup]\nfifo_size=64KB\n", 1, NULL },
    { "2 fifo_size=64K ok",
      "[backup]\nfifo_size=64K\n", 1, NULL },

    /* item 3: buffer_disk_limit vs max(pipe capacity for fifo_size, io_size) */
    { "3 mem=64MB disk=16KB fails",
      "[backup]\nbuffer_memory_size=64MB\nbuffer_disk_limit=16KB\nbuffer_disk_path=@SPOOL@\n", 0, "is below max(pipe capacity for fifo_size, io_size)" },
    { "3 fifo=1MB disk=512KB fails (s01 C2)",
      "[backup]\nfifo_size=1MB\nbuffer_memory_size=1MB\nbuffer_disk_limit=512KB\nbuffer_disk_path=@SPOOL@\n", 0, "buffer_disk_limit=524288 is below" },
    { "3 fifo=1MB disk=1MB ok (boundary)",
      "[backup]\nfifo_size=1MB\nbuffer_memory_size=1MB\nbuffer_disk_limit=1MB\nbuffer_disk_path=@SPOOL@\n", 1, NULL },
    { "3 fifo=768KB disk=800KB fails (kernel rounds the pipe up to 1MB)",
      "[backup]\nfifo_size=768KB\nbuffer_memory_size=1MB\nbuffer_disk_limit=800KB\nbuffer_disk_path=@SPOOL@\n", 0,
      "buffer_disk_limit=819200 is below max(pipe capacity for fifo_size, io_size)" },
    { "3 fifo=768KB disk=1MB ok (rounded pipe capacity)",
      "[backup]\nfifo_size=768KB\nbuffer_memory_size=1MB\nbuffer_disk_limit=1MB\nbuffer_disk_path=@SPOOL@\n", 1, NULL },
    { "3 disk without mem still fails (s01 C5)",
      "[backup]\nbuffer_memory_size=0\nbuffer_disk_limit=256MB\nbuffer_disk_path=@SPOOL@\n", 0,
      "buffer_disk_limit=268435456 requires buffer_memory_size > 0" },
    { "3 mem below io_size still fails (s01 B5)",
      "[backup]\nbuffer_memory_size=1KB\n", 0, "buffer_memory_size=1024 is below the I/O unit" },
    { "3 disk path missing dir still fails (s02 163)",
      "[backup]\nbuffer_memory_size=256MB\nbuffer_disk_limit=256MB\nbuffer_disk_path=/nonexistent_dir_xyz/spool\n", 0,
      "buffer_disk_path='/nonexistent_dir_xyz/spool' must be an existing directory" },

    /* item 5: buffer_disk_path must be absolute */
    { "5 relative path with disk tier fails",
      "[backup]\nbuffer_memory_size=1MB\nbuffer_disk_limit=64MB\nbuffer_disk_path=./spool\n", 0, "buffer_disk_path must be an absolute path: './spool'" },
    { "5 relative path ignored when disk_limit=0 (item 4 out of scope)",
      "[backup]\nbuffer_disk_limit=0\nbuffer_disk_path=spool\n", 1, NULL },
    { "5 absolute non-existent path with disk_limit=0 ok (legacy conf)",
      "[backup]\nbuffer_disk_limit=0\nbuffer_disk_path=/var/tmp/cubrid_backup_spool\n", 1, NULL },

    /* sections */
    { "S unknown section fails",
      "[backup]\nthread_count=3\n[foo]\nthread_count=abc\nsa_mode=true\n[restore]\npartial_recovery=true\n",
      0, "cubrid_backup.conf:3: unknown section [foo]" },
    { "S misspelled section fails",
      "[bakup]\nthread_count=8\n", 0, "cubrid_backup.conf:1: unknown section [bakup]" },
    { "S upper/mixed-case headers accepted",
      "[BACKUP]\nsa_mode=true\n[ Restore ]\nuse_database_location_path=true\n", 1, NULL },
    { "S key before any header still fails",
      "sa_mode=true\n[backup]\n", 0, "cubrid_backup.conf:1: option outside of any section: sa_mode=true" },
    { "S unknown key still fails",
      "[backup]\nno_such_key=1\n", 0, "invalid [backup] option: no_such_key=1" },
    { "S unknown section first, then [backup]",
      "[common]\nfoo=bar\n[backup]\nsa_mode=true\n", 0, "cubrid_backup.conf:1: unknown section [common]" },

    /* line syntax follows cubrid.conf: a line is applied or init fails */
    { "M inline # comment applied",
      "[backup]\nthread_count = 8 # comment\n# comment\n  # indented comment\n", 1, NULL },
    { "M inline ; comment applied",
      "[backup]\nthread_count=8 ; comment\n", 1, NULL },
    { "M comment before any section ok",
      "# header comment\n\n[backup]\nsa_mode=true\n", 1, NULL },
    { "M space inside a size value fails",
      "[backup]\nbuffer_memory_size=64 MB\n", 0, "cubrid_backup.conf:2: invalid [backup] option: buffer_memory_size=64 MB" },
    { "M space inside an int value fails",
      "[backup]\nsleep_msecs=1 0\n", 0, "invalid [backup] option: sleep_msecs=1 0" },
    { "M missing '=' fails",
      "[backup]\nthread_count 8\n", 0, "cubrid_backup.conf:2: syntax error: thread_count 8" },
    { "M hyphenated key fails",
      "[backup]\nthread-count=8\n", 0, "invalid [backup] option: thread-count=8" },
    { "M key with digit fails",
      "[backup]\nthread_count2=8\n", 0, "invalid [backup] option: thread_count2=8" },
    { "M ; comment line fails",
      "[backup]\n; memo\n", 0, "cubrid_backup.conf:2: syntax error: ; memo" },
    { "M empty key fails",
      "[backup]\n=8\n", 0, "cubrid_backup.conf:2: syntax error: =8" },
    { "M empty int value fails",
      "[backup]\nthread_count=\n", 0, "invalid [backup] option: thread_count=" },
    { "M value that is only a comment fails",
      "[backup]\nthread_count= # none\n", 0, "invalid [backup] option: thread_count=" },
    { "M empty bool value fails",
      "[backup]\nsa_mode=\n", 0, "invalid [backup] option: sa_mode=" },
    { "M empty size value fails",
      "[backup]\nbuffer_memory_size=\n", 0, "invalid [backup] option: buffer_memory_size=" },
    { "M empty path value fails",
      "[backup]\nbuffer_disk_path=\n", 0, "invalid [backup] option: buffer_disk_path=" },
    { "M double-quoted value fails",
      "[backup]\nthread_count=\"8\"\n", 0, "quoted values are not supported: thread_count=\"8\"" },
    { "M single-quoted path fails",
      "[backup]\nbuffer_disk_path='/var/tmp'\n", 0, "quoted values are not supported" },
    { "M duplicate key fails",
      "[backup]\nthread_count=4\nthread_count=8\n", 0, "cubrid_backup.conf:3: duplicate option: thread_count" },
    { "M duplicate key across repeated [backup] fails",
      "[backup]\nsa_mode=true\n[restore]\npartial_recovery=true\n[backup]\nSA_MODE=false\n", 0, "duplicate option: SA_MODE" },
    { "M keys in different sections are independent",
      "[backup]\nsa_mode=true\n[restore]\npartial_recovery=true\n", 1, NULL },
    { "M garbage line fails",
      "[backup]\nsa_mode=true\nhello world\n", 0, "cubrid_backup.conf:3: syntax error: hello world" },
    { "M unterminated header fails",
      "[backup\nsa_mode=true\n", 0, "cubrid_backup.conf:1: syntax error: [backup" },
    { "M CRLF line endings ok",
      "[backup]\r\nthread_count=8\r\n", 1, NULL },

    /* conf absent */
    { "C no conf file -> defaults", NULL, 1, NULL },
};

static void t_init_cases (void)
{
    char msg[512];
    size_t i;

    printf ("== init cases\n");
    for (i = 0; i < sizeof (init_cases) / sizeof (init_cases[0]); i++)
    {
        const struct init_case* c = &init_cases[i];
        int rc = run_conf (c->conf);

        snprintf (msg, sizeof (msg), "%s: rc=%s", c->name, c->ok ? "SUCCESS" : "FAILURE");
        CHECK ((rc == SUCCESS) == c->ok, msg);

        if (c->log_needle != NULL)
        {
            snprintf (msg, sizeof (msg), "%s: log has \"%s\"", c->name, c->log_needle);
            CHECK (log_has (c->log_needle), msg);
        }
    }
}

/* ── 3. value checks for cases that pass ── */

static void t_values (void)
{
    char buf[256];

    printf ("== parsed values\n");

    CHECK (run_conf ("[backup]\nbuffer_memory_size=1.5G\n") == SUCCESS
           && bo ()->buffer_memory_size == 3 * GB / 2, "buffer_memory_size=1.5G -> 1610612736");
    CHECK (run_conf ("[backup]\nbuffer_memory_size=2t\n") == SUCCESS
           && bo ()->buffer_memory_size == 2 * TB, "buffer_memory_size=2t -> 2TB (lower-case unit)");
    CHECK (run_conf ("[backup]\nbuffer_memory_size=1P\n") == SUCCESS
           && bo ()->buffer_memory_size == PB, "buffer_memory_size=1P -> 1PB");

    /* fifo_size above pipe-max-size: clamped to pipe-max-size + WARN */
    snprintf (buf, sizeof (buf), "[backup]\nfifo_size=%dB\n", g_pipe_max + 4096);
    CHECK (run_conf (buf) == SUCCESS && bo ()->fifo_size == g_pipe_max, "fifo_size > pipe-max-size clamped to pipe-max-size");
    CHECK (log_has ("exceeds pipe-max-size"), "fifo_size clamp WARN logged");

    CHECK (run_conf ("[backup]\nfifo_size=8MB\n") == SUCCESS
           && bo ()->fifo_size == (g_pipe_max < 8 * MB ? g_pipe_max : 8 * MB), "fifo_size=8MB -> min(8MB, pipe-max-size) (s01 A2)");

    snprintf (buf, sizeof (buf), "[backup]\nfifo_size=%d\n", g_pipe_max);
    CHECK (run_conf (buf) == SUCCESS && bo ()->fifo_size == g_pipe_max && !log_has ("exceeds pipe-max-size"),
           "fifo_size == pipe-max-size kept without WARN");

    /* item 3 boundary uses io_size when io_size > the pipe capacity for fifo_size */
    {
        int io;
        run_conf (NULL);
        io = backup_mgr->io_size;
        if (io > 64 * 1024)
        {
            snprintf (buf, sizeof (buf), "[backup]\nbuffer_memory_size=64MB\nbuffer_disk_limit=%dB\nbuffer_disk_path=@SPOOL@\n", io - 1);
            CHECK (run_conf (buf) == FAILURE, "disk_limit = io_size-1 fails (io_size > pipe capacity)");
        }
        else
        {
            snprintf (buf, sizeof (buf), "[backup]\nbuffer_memory_size=64MB\nbuffer_disk_limit=%dB\nbuffer_disk_path=@SPOOL@\n", 64 * 1024 - 1);
            CHECK (run_conf (buf) == FAILURE, "disk_limit = pipe capacity-1 fails (64KB fifo, capacity >= io_size)");
        }
        snprintf (buf, sizeof (buf), "[backup]\nbuffer_memory_size=64MB\nbuffer_disk_limit=%dB\nbuffer_disk_path=@SPOOL@\n",
                  io > 64 * 1024 ? io : 64 * 1024);
        CHECK (run_conf (buf) == SUCCESS, "disk_limit = max(pipe capacity for fifo_size, io_size) ok");
    }


    CHECK (run_conf ("[BACKUP]\nsa_mode=true\n[ Restore ]\nuse_database_location_path=true\n") == SUCCESS
           && bo ()->sa_mode == true && ro ()->use_database_location_path == true, "[BACKUP]/[ Restore ] values applied");

    CHECK (run_conf ("[backup]\nthread_count = 8 # comment\n") == SUCCESS && bo ()->thread_count == 8,
           "inline # comment: value before it applied");
    CHECK (run_conf ("[backup]\nthread_count=8;x\nsleep_msecs = 20\t\r\n") == SUCCESS
           && bo ()->thread_count == 8 && bo ()->sleep_msecs == 20,
           "inline ; comment and surrounding blanks/CR stripped");
    {
        char p[PATH_MAX + 64];
        char sp[PATH_MAX];

        /* spool dir with characters the old value regex rejected (Jenkins job@2 workspaces) */
        snprintf (sp, sizeof (sp), "%s/job@2 + ~dir \xED\x95\x9C\xEA\xB8\x80", g_home);
        assert (mkdir (sp, 0700) == 0);
        snprintf (p, sizeof (p), "[backup]\nbuffer_memory_size=1MB\nbuffer_disk_limit=64MB\nbuffer_disk_path=%s\n", sp);
        CHECK (run_conf (p) == SUCCESS && strcmp (bo ()->buffer_disk_path, sp) == 0,
               "spool path with '@', '+', '~', spaces and non-ASCII applied as written");
        rmdir (sp);
    }
    CHECK (run_conf ("[backup]\nbuffer_disk_limit=0\nbuffer_disk_path=/a=b/c\n") == SUCCESS
           && strcmp (bo ()->buffer_disk_path, "/a=b/c") == 0, "value may contain '='");

    CHECK (run_conf (NULL) == SUCCESS && bo ()->fifo_size == 64 * 1024 && bo ()->buffer_memory_size == 0
           && bo ()->buffer_disk_limit == 0 && bo ()->buffer_disk_path[0] == '\0', "no conf -> defaults");

    /* working directory is logged at INFO */
    snprintf (buf, sizeof (buf), "INFO: backup working directory: %s/tmp/.cubrid_backup", g_home);
    CHECK (run_conf (NULL) == SUCCESS && log_has (buf), "chosen working directory logged at INFO");
}

/* ── 4. long lines ── */

static void t_long_lines (void)
{
    static char text[4096];
    char* p;

    printf ("== line length\n");

    /* comment of exactly 1023 chars + '\n', then a key: fits */
    p = text;
    p += sprintf (p, "[backup]\n#");
    memset (p, 'x', 1022); p += 1022;
    p += sprintf (p, "\nsa_mode=true\n");
    CHECK (run_conf (text) == SUCCESS && bo ()->sa_mode == true, "1023-char line accepted, next line parsed");

    /* last line of exactly 1023 chars without '\n' */
    p = text;
    p += sprintf (p, "[backup]\nsa_mode=true\n#");
    memset (p, 'x', 1022); p += 1022;
    *p = '\0';
    CHECK (run_conf (text) == SUCCESS, "1023-char last line without newline accepted");

    /* 1024-char line fails */
    p = text;
    p += sprintf (p, "[backup]\n#");
    memset (p, 'x', 1023); p += 1023;
    p += sprintf (p, "\n");
    CHECK (run_conf (text) == FAILURE && log_has ("cubrid_backup.conf:2: line longer than 1023 characters"),
           "1024-char line rejected with line number");

    /* the old bug: the tail of an over-long comment was parsed as a key */
    p = text;
    p += sprintf (p, "[backup]\n#");
    memset (p, 'x', 1022); p += 1022;
    p += sprintf (p, "sa_mode=true\n");
    CHECK (run_conf (text) == FAILURE, "over-long comment tail is not parsed as sa_mode=true");

    /* 1024-char line at EOF without newline also fails */
    p = text;
    p += sprintf (p, "[backup]\n#");
    memset (p, 'x', 1023); p += 1023;
    *p = '\0';
    CHECK (run_conf (text) == FAILURE, "1024-char last line without newline rejected");

    /* CRLF: the '\r' terminator is not counted as line content */
    p = text;
    p += sprintf (p, "[backup]\r\n#");
    memset (p, 'x', 1022); p += 1022;
    p += sprintf (p, "\r\nsa_mode=true\r\n");
    CHECK (run_conf (text) == SUCCESS && bo ()->sa_mode == true, "1023-char CRLF line accepted, next line parsed");

    p = text;
    p += sprintf (p, "[backup]\r\n#");
    memset (p, 'x', 1023); p += 1023;
    p += sprintf (p, "\r\n");
    CHECK (run_conf (text) == FAILURE && log_has ("cubrid_backup.conf:2: line longer than 1023 characters"),
           "1024-char CRLF line rejected");

    /* a NUL byte inside a line is rejected (strlen would hide the rest) */
    {
        static const char nul_conf[] = "[backup]\nsa_mode=true\0junk\nfifo_size=256KB\n";
        FILE* fp = fopen (g_conf, "w");
        assert (fp != NULL);
        fwrite (nul_conf, 1, sizeof (nul_conf) - 1, fp);
        fclose (fp);
        CHECK (run_init () == FAILURE && log_has ("cubrid_backup.conf:2: line contains a NUL byte"),
               "line with a NUL byte rejected");
    }
}

/* ── 5. unreadable conf / too-long conf path / CUBRID_TMP ── */

static void t_env_cases (void)
{
    char path[PATH_MAX];
    char saved_home[PATH_MAX];

    printf ("== conf file access / paths / CUBRID_TMP\n");

    /* conf is a directory: fopen succeeds, read fails (EISDIR) */
    unlink (g_conf);
    assert (mkdir (g_conf, 0700) == 0);
    CHECK (run_init () == FAILURE && log_has ("read error"), "conf that cannot be read (directory) fails");
    rmdir (g_conf);

    /* conf without read permission (skipped for root) */
    if (geteuid () != 0)
    {
        write_conf ("[backup]\nsa_mode=true\n");
        chmod (g_conf, 0);
        CHECK (run_init () == FAILURE && log_has ("cannot open"), "conf without read permission fails");
        chmod (g_conf, 0600);
    }
    else
    {
        printf ("  [SKIP] conf without read permission (running as root)\n");
    }
    unlink (g_conf);

    /* $CUBRID so long that $CUBRID/conf/cubrid_backup.conf exceeds PATH_MAX-1
     * while $CUBRID/log/cubrid_backup.log (2 chars shorter) still fits. */
    {
        const int target = PATH_MAX - 1 - (int) strlen ("/conf/cubrid_backup.conf");   /* 4071 */
        char deep[PATH_MAX];
        char comp[256];
        int len;

        snprintf (deep, sizeof (deep), "%s/deep", g_spool);
        mkdir (deep, 0700);
        len = (int) strlen (deep);

        memset (comp, 'd', 200);
        comp[200] = '\0';
        while (len + 1 + 200 + 1 < target)
        {
            len += sprintf (deep + len, "/%s", comp);
            mkdir (deep, 0700);
        }
        memset (comp, 'e', (size_t) (target - len - 1));
        comp[target - len - 1] = '\0';
        len += sprintf (deep + len, "/%s", comp);
        assert (len == target);
        assert (mkdir (deep, 0700) == 0);
        snprintf (path, sizeof (path), "%s/conf", deep); mkdir (path, 0700);
        snprintf (path, sizeof (path), "%s/tmp", deep);  mkdir (path, 0700);
        snprintf (path, sizeof (path), "%s/log", deep);  mkdir (path, 0700);

        setenv ("CUBRID", deep, 1);
        snprintf (path, sizeof (path), "%s/log/cubrid_backup.log", deep);
        {
            FILE* fp = fopen (path, "w");
            if (fp != NULL) fclose (fp);
        }
        CHECK (start_backup_manager () == FAILURE, "conf path too long fails");
        stop_backup_manager ();
        {
            /* log_has reads g_log; point it at the deep log temporarily */
            strcpy (saved_home, g_log);
            strcpy (g_log, path);
            CHECK (log_has ("cubrid_backup.conf path is too long"), "conf path too long logged");
            strcpy (g_log, saved_home);
        }
        setenv ("CUBRID", g_home, 1);
    }

    /* CUBRID_TMP is consulted only when $CUBRID/tmp is unusable */
    snprintf (path, sizeof (path), "%s/tmp/.cubrid_backup", g_home);
    rmdir (path);
    snprintf (path, sizeof (path), "%s/tmp", g_home);
    assert (rmdir (path) == 0);

    setenv ("CUBRID_TMP", "relative_tmp", 1);
    CHECK (run_conf (NULL) == FAILURE && log_has ("CUBRID_TMP must be an absolute path: 'relative_tmp'"),
           "relative CUBRID_TMP fails");

    /* empty CUBRID_TMP is treated as unset and falls back to /tmp; run it only when
     * /tmp/.cubrid_backup already exists so the test leaves no new system state */
    setenv ("CUBRID_TMP", "", 1);
    if (access ("/tmp/.cubrid_backup", R_OK | W_OK | X_OK) == 0)
    {
        CHECK (run_conf (NULL) == SUCCESS && !log_has ("CUBRID_TMP must be an absolute path")
               && log_has ("INFO: backup working directory: /tmp/.cubrid_backup"),
               "empty CUBRID_TMP falls back to /tmp");
    }
    else
    {
        printf ("  SKIP empty CUBRID_TMP: /tmp/.cubrid_backup does not exist or is not usable\n");
    }

    setenv ("CUBRID_TMP", g_spool, 1);
    {
        char needle[PATH_MAX + 64];
        snprintf (needle, sizeof (needle), "INFO: backup working directory: %s/.cubrid_backup", g_spool);
        CHECK (run_conf (NULL) == SUCCESS && log_has (needle), "absolute CUBRID_TMP used and logged");
    }
    unsetenv ("CUBRID_TMP");

    assert (mkdir (path, 0700) == 0);
}

/* ── 6. legacy confs from testcases/ (must keep passing) ── */

static const struct init_case legacy_cases[] =
{
    { "L testcases/cubrid_backup.conf",
      "[backup]\nremove_archive=false\nsa_mode=false\nno_check=true\nthread_count=8\ncompress=false\n"
      "except_active_log=false\nsleep_msecs=20\n\n"
      "# tiered buffer (Linux/POSIX only). Absent keys fall back to these defaults.\n"
      "# buffer_memory_size=0 disables buffering entirely (legacy direct-FIFO path).\n"
      "fifo_size=64KB\nbuffer_memory_size=64MB\nbuffer_disk_limit=0\n"
      "buffer_disk_path=/var/tmp/cubrid_backup_spool\nbuffer_disk_keep_spool=false\n", 1, NULL },
    { "L run_test.sh tc05 (absolute spool)",
      "[backup]\nremove_archive=false\nsa_mode=false\nno_check=true\nthread_count=1\ncompress=true\n"
      "fifo_size=64KB\nbuffer_memory_size=1MB\nbuffer_disk_limit=64MB\nbuffer_disk_path=@SPOOL@\nbuffer_disk_keep_spool=false\n", 1, NULL },
#define SWEEP(fifo, mem, disk, keep) \
      "[backup]\nremove_archive=false\nsa_mode=false\nno_check=true\nthread_count=1\ncompress=true\n" \
      "except_active_log=false\nsleep_msecs=0\nfifo_size=" fifo "\nbuffer_memory_size=" mem \
      "\nbuffer_disk_limit=" disk "\nbuffer_disk_path=@SPOOL@\nbuffer_disk_keep_spool=" keep "\n"
    { "L s01 A1 fifo=64KB",       SWEEP ("64KB", "1MB", "256MB", "false"), 1, NULL },
    { "L s01 A2 fifo=8MB",        SWEEP ("8MB", "1MB", "256MB", "false"), 1, NULL },
    { "L s01 A3 fifo=abc",        SWEEP ("abc", "1MB", "256MB", "false"), 0, NULL },
    { "L s01 A4 fifo=-1",         SWEEP ("-1", "1MB", "256MB", "false"), 0, NULL },
    { "L s01 A5 fifo=3GB",        SWEEP ("3GB", "1MB", "256MB", "false"), 0, NULL },
    { "L s01 B1 mem=0 disk=0",    SWEEP ("1MB", "0", "0", "false"), 1, NULL },
    { "L s01 B2 mem=64KB",        SWEEP ("1MB", "64KB", "256MB", "false"), 1, NULL },
    { "L s01 B3 mem=512MB",       SWEEP ("1MB", "512MB", "256MB", "false"), 1, NULL },
    { "L s01 B4 mem=5QB",         SWEEP ("1MB", "5QB", "256MB", "false"), 0, NULL },
    { "L s01 B5 mem=16KB",        SWEEP ("1MB", "16KB", "256MB", "false"), 0, NULL },
    { "L s01 C1 disk=0",          SWEEP ("1MB", "1MB", "0", "false"), 1, NULL },
    { "L s01 C3 disk=256MB",      SWEEP ("1MB", "1MB", "256MB", "false"), 1, NULL },
    { "L s01 C4 disk=-5",         SWEEP ("1MB", "1MB", "-5", "false"), 0, NULL },
    { "L s01 C5 disk w/o mem",    SWEEP ("1MB", "0", "256MB", "false"), 0, NULL },
    { "L s01 D1 keep=true",       SWEEP ("1MB", "1MB", "256MB", "true"), 1, NULL },
    { "L s02 mem=1MB disk=4MB",   SWEEP ("1MB", "1MB", "4MB", "false"), 1, NULL },
    { "L s02 disk=100GB (free-space WARN only)", SWEEP ("1MB", "1MB", "100GB", "false"), 1, NULL },
    { "L s03 mem=1MB disk=4MB",   SWEEP ("1MB", "1MB", "4MB", "false"), 1, NULL },
    { "L s03 mem=64KB disk=256MB",SWEEP ("1MB", "64KB", "256MB", "false"), 1, NULL },
    { "L s04 mem=64MB disk=8GB",  SWEEP ("1MB", "64MB", "8GB", "false"), 1, NULL },
    { "L s05 mem=256MB disk=16GB",SWEEP ("1MB", "256MB", "16GB", "false"), 1, NULL },
    { "L s10 disk=3221225472",    SWEEP ("1MB", "64MB", "3221225472", "false"), 1, NULL },
    { "L logphase-e2e 4MB/256MB", SWEEP ("1MB", "4MB", "256MB", "false"), 1, NULL },
#undef SWEEP
    { "L s07 baseline (no buffer keys)",
      "[backup]\nremove_archive=false\nsa_mode=false\nno_check=true\nthread_count=1\ncompress=true\n"
      "except_active_log=false\nsleep_msecs=0\n", 1, NULL },
    { "L s09 (no fifo key)",
      "[backup]\nremove_archive=false\ncompress=true\nthread_count=1\nbuffer_memory_size=4MB\n"
      "buffer_disk_limit=1GB\nbuffer_disk_path=@SPOOL@\n", 1, NULL },
    { "L restore section",
      "[backup]\nsa_mode=true\n[restore]\npartial_recovery=false\nuse_database_location_path=true\n", 1, NULL },
};

static void t_legacy (void)
{
    char msg[512];
    size_t i;

    printf ("== legacy confs\n");
    for (i = 0; i < sizeof (legacy_cases) / sizeof (legacy_cases[0]); i++)
    {
        int rc = run_conf (legacy_cases[i].conf);

        snprintf (msg, sizeof (msg), "%s: rc=%s", legacy_cases[i].name, legacy_cases[i].ok ? "SUCCESS" : "FAILURE");
        CHECK ((rc == SUCCESS) == legacy_cases[i].ok, msg);
    }
}

static void rm_rf (const char* dir)
{
    char cmd[PATH_MAX + 32];

    snprintf (cmd, sizeof (cmd), "rm -rf '%s'", dir);
    if (system (cmd) != 0)
    {
        printf ("  [WARN] cleanup of %s failed\n", dir);
    }
}

int main (void)
{
    const char* tmpdir = getenv ("TMPDIR");
    char path[PATH_MAX];
    FILE* fp;

    snprintf (g_home, sizeof (g_home), "%s/conf_ut.XXXXXX", (tmpdir != NULL && tmpdir[0] == '/') ? tmpdir : "/tmp");
    if (mkdtemp (g_home) == NULL)
    {
        perror ("mkdtemp");
        return 2;
    }

    snprintf (path, sizeof (path), "%s/conf", g_home); mkdir (path, 0700);
    snprintf (path, sizeof (path), "%s/log", g_home);  mkdir (path, 0700);
    snprintf (path, sizeof (path), "%s/tmp", g_home);  mkdir (path, 0700);
    snprintf (g_spool, sizeof (g_spool), "%s/spool", g_home); mkdir (g_spool, 0700);
    snprintf (g_conf, sizeof (g_conf), "%s/conf/cubrid_backup.conf", g_home);
    snprintf (g_log, sizeof (g_log), "%s/log/cubrid_backup.log", g_home);

    setenv ("CUBRID", g_home, 1);
    unsetenv ("CUBRID_TMP");

    g_pipe_max = 1024 * 1024;
    fp = fopen ("/proc/sys/fs/pipe-max-size", "r");
    if (fp != NULL)
    {
        if (fscanf (fp, "%d", &g_pipe_max) != 1)
        {
            g_pipe_max = 1024 * 1024;
        }
        fclose (fp);
    }
    printf ("CUBRID=%s pipe-max-size=%d\n", g_home, g_pipe_max);

    t_size_parser ();
    t_int_parser ();
    t_init_cases ();
    t_values ();
    t_long_lines ();
    t_env_cases ();
    t_legacy ();

    rm_rf (g_home);

    printf ("\nconf_ut: %d checks, %d failed -> %s\n", g_checks, g_fail, g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
