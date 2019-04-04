#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "cubrid_backup_api.h"

char *db_name;

void usage ()
{
    printf ("./backup_tc03 [DB_NAME]\n\n");
    printf ("ex)\n");
    printf ("backup (full) ==> ./backup_tc03 demodb\n");
}

void pass_invalid_backup_info (void)
{
    CUBRID_BACKUP_INFO backup_info;
    void *backup_handle = NULL;

    // test #1 - invalid dbname
    // --> This is not a API error... cub_server returns error
    backup_info.db_name        = "NANANA";
    backup_info.backup_level   = 0;
    backup_info.remove_archive = -1;
    backup_info.sa_mode        = -1;
    backup_info.no_check       = -1;
    backup_info.compress       = -1;

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_backup_begin (&backup_info, &backup_handle))
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }
    else
    {
        printf ("[OK] %s\n", __func__);
    }

    cubrid_backup_finalize ();

    // test #2 - invalid backup_level
    backup_info.db_name        = db_name;
    backup_info.backup_level   = 1004;
    backup_info.remove_archive = -1;
    backup_info.sa_mode        = -1;
    backup_info.no_check       = -1;
    backup_info.compress       = -1;

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_backup_begin (&backup_info, &backup_handle))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    cubrid_backup_finalize ();
}

void pass_invalid_backup_info_2 (void)
{
    CUBRID_BACKUP_INFO backup_info;
    void *backup_handle = NULL;

    // test #1 - invalid remove_archive
    backup_info.db_name        = db_name;
    backup_info.backup_level   = 0;
    backup_info.remove_archive = 7; // -1, 0, 1
    backup_info.sa_mode        = -1;
    backup_info.no_check       = -1;
    backup_info.compress       = -1;

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_backup_begin (&backup_info, &backup_handle))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    cubrid_backup_finalize ();

    // test #2 - invalid sa_mode
    backup_info.db_name        = db_name;
    backup_info.backup_level   = 0;
    backup_info.remove_archive = -1;
    backup_info.sa_mode        = -20; // -1, 0, 1
    backup_info.no_check       = -1;
    backup_info.compress       = -1;

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_backup_begin (&backup_info, &backup_handle))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    cubrid_backup_finalize ();

    // test #3 - invalid no_check
    backup_info.db_name        = db_name;
    backup_info.backup_level   = 0;
    backup_info.remove_archive = -1;
    backup_info.sa_mode        = -1;
    backup_info.no_check       = 77; // -1, 0, 1
    backup_info.compress       = -1;

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_backup_begin (&backup_info, &backup_handle))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    cubrid_backup_finalize ();

    // test #4 - invalid compress
    backup_info.db_name        = db_name;
    backup_info.backup_level   = 0;
    backup_info.remove_archive = -1;
    backup_info.sa_mode        = -1;
    backup_info.no_check       = -1;
    backup_info.compress       = -99; // -1, 0, 1

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_backup_begin (&backup_info, &backup_handle))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    cubrid_backup_finalize ();
}

void pass_null_backup_handle (void)
{
    CUBRID_BACKUP_INFO backup_info;
    void *backup_handle = NULL;

    char backup_data_buffer[4096];
    int backup_data_size = 0;

    backup_info.db_name        = db_name;
    backup_info.backup_level   = 0;
    backup_info.remove_archive = -1;
    backup_info.sa_mode        = -1;
    backup_info.no_check       = -1;
    backup_info.compress       = -1;

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_backup_begin (&backup_info, &backup_handle))
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    // test #1-1 - NULL backup handle (cubrid_backup_read)
    if (-1 == cubrid_backup_read (NULL, backup_data_buffer, 4096, &backup_data_size))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    // test #1-2 - NULL backup handle (cubrid_backup_end)
    if (-1 == cubrid_backup_end (NULL))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    cubrid_backup_finalize ();
}

void pass_invalid_backup_handle (void)
{
    CUBRID_BACKUP_INFO backup_info;
    void *backup_handle = NULL;
    void *invalid_backup_handle;

    char backup_data_buffer[4096];
    int backup_data_size = 0;

    backup_info.db_name        = db_name;
    backup_info.backup_level   = 0;
    backup_info.remove_archive = -1;
    backup_info.sa_mode        = -1;
    backup_info.no_check       = -1;
    backup_info.compress       = -1;

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_backup_begin (&backup_info, &backup_handle))
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    // test #1-1 - invalid backup handle (cubrid_backup_read)
    //
    // invalid_backup_handle 포인터가 엄한 메모리를 참조하고 있는 상태이다.
    invalid_backup_handle = backup_data_buffer;

    if (-1 == cubrid_backup_read (invalid_backup_handle, backup_data_buffer, 4096, &backup_data_size))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    // test #1-2 - invalid backup handle (cubrid_backup_end)
    if (-1 == cubrid_backup_end (invalid_backup_handle))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    cubrid_backup_finalize ();
}

void pass_invalid_backup_args (void)
{
    CUBRID_BACKUP_INFO backup_info;
    void *backup_handle = NULL;

    char backup_data_buffer[4096];
    int backup_data_size = 0;

    backup_info.db_name        = db_name;
    backup_info.backup_level   = 0;
    backup_info.remove_archive = -1;
    backup_info.sa_mode        = -1;
    backup_info.no_check       = -1;
    backup_info.compress       = -1;

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_backup_begin (&backup_info, &backup_handle))
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    // test #1-1 - backup buffer == NULL
    //
    // if sizeof (backup_data_buffer) != 3rd param, then it is seg fault.
    // it's a user level bug... so I don't care of it.
    if (-1 == cubrid_backup_read (backup_handle, NULL, 4096, &backup_data_size))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    // test #1-2 - out parameter (backup data length) == NULL
    if (-1 == cubrid_backup_read (backup_handle, backup_data_buffer, 4096, NULL))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    cubrid_backup_finalize ();
}

int main (int argc, char *argv[])
{
    if (argc != 2)
    {
        usage ();
        exit (1);
    }

    db_name = argv[1];

    /* test - invalid backup arguments */
    pass_invalid_backup_info ();

    /* RND-960 */
    pass_invalid_backup_info_2 ();

    pass_null_backup_handle ();
    sleep(1);

    pass_invalid_backup_handle ();

    pass_invalid_backup_args ();

    return 0;
}
