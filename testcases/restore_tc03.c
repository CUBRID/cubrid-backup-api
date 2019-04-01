#include <stdio.h>
#include <stdlib.h>
#include "cubrid_backup_api.h"

CUBRID_RESTORE_INFO cub_restore_info;
void *cub_restore_handle = NULL;
char *db_name;

void usage ()
{
    printf ("./restore_tc03 [DB_NAME]\n\n");
    printf ("ex)\n");
    printf ("restore (full)    ==> ./restore_tc03 demodb\n");
    printf ("        (level 1) ==> ./restore_tc03 demodb\n");
    printf ("        (level 2) ==> ./restore_tc03 demodb\n");
}

void pass_invalid_restore_info (void)
{
    CUBRID_RESTORE_INFO restore_info;
    void *restore_handle = NULL;

    // test #1 - invalid dbname
    // --> in restore to file case ... db_name is not important. just use restore file name.
    restore_info.db_name = "NANANA";
    restore_info.backup_level = 0;
    restore_info.restore_type = RESTORE_TO_FILE;
    restore_info.backup_file_path = "./restore_dir";

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_restore_begin (&restore_info, &restore_handle))
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
    restore_info.db_name = db_name;
    restore_info.backup_level = 7; // 0 ~ 3 range
    restore_info.restore_type = RESTORE_TO_FILE;
    restore_info.backup_file_path = "./restore_dir";

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_restore_begin (&restore_info, &restore_handle))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    cubrid_backup_finalize ();

    // test #3 - invalid restore_type 
    restore_info.db_name = db_name;
    restore_info.backup_level = 0;
    restore_info.restore_type = 88; // 0 ~ 1 is vaild range
    restore_info.backup_file_path = "./restore_dir";

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_restore_begin (&restore_info, &restore_handle))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    cubrid_backup_finalize ();

    // test #4 - invalid backup file path: no permission
    restore_info.db_name = db_name;
    restore_info.backup_level = 0;
    restore_info.restore_type = RESTORE_TO_FILE; // 0 ~ 1 is vaild range
    restore_info.backup_file_path = "/home";

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_restore_begin (&restore_info, &restore_handle))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    cubrid_backup_finalize ();

    // test #5 - invalid backup file path: not exist 
    restore_info.db_name = db_name;
    restore_info.backup_level = 0;
    restore_info.restore_type = RESTORE_TO_FILE; // 0 ~ 1 is vaild range
    restore_info.backup_file_path = "./RURURURU";

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_restore_begin (&restore_info, &restore_handle))
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

void pass_null_restore_handle (void)
{
    CUBRID_RESTORE_INFO restore_info;
    void *restore_handle = NULL;

    char restore_data_buffer[4096];

    restore_info.db_name = db_name;
    restore_info.backup_level = 0;
    restore_info.restore_type = RESTORE_TO_FILE;
    restore_info.backup_file_path = "./restore_dir";

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_restore_begin (&restore_info, &restore_handle))
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    // test #1-1 - NULL backup handle (cubrid_backup_read)
    if (-1 == cubrid_restore_write (NULL, restore_info.backup_level, restore_data_buffer, 4096))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    // test #1-2 - NULL backup handle (cubrid_backup_end)
    if (-1 == cubrid_restore_end (NULL))
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

void pass_invalid_restore_handle (void)
{
    CUBRID_RESTORE_INFO restore_info;
    void *restore_handle = NULL;
    void *invalid_restore_handle;

    char restore_data_buffer[4096];

    restore_info.db_name = db_name;
    restore_info.backup_level = 0;
    restore_info.restore_type = RESTORE_TO_FILE;
    restore_info.backup_file_path = "./restore_dir";

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_restore_begin (&restore_info, &restore_handle))
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    // test #1-1 - invalid backup handle (cubrid_restore_write)
    //
    // invalid_restore_handle 포인터가 엄한 메모리를 참조하고 있는 상태이다.
    invalid_restore_handle = restore_data_buffer;

    if (-1 == cubrid_restore_write (invalid_restore_handle, restore_info.backup_level, restore_data_buffer, 4096))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    // test #1-2 - invalid backup handle (cubrid_restore_end)
    if (-1 == cubrid_restore_end (invalid_restore_handle))
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

void pass_invalid_restore_args (void)
{
    CUBRID_RESTORE_INFO restore_info;
    void *restore_handle = NULL;

    char restore_data_buffer[4096];

    restore_info.db_name = db_name;
    restore_info.backup_level = 0;
    restore_info.restore_type = RESTORE_TO_FILE;
    restore_info.backup_file_path = "./restore_dir";

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_restore_begin (&restore_info, &restore_handle))
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    // test #1-1 - pass invalid backup level
    if (-1 == cubrid_restore_write (restore_handle, 77, restore_data_buffer, 4096))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    // test #1-2 - restore data buffer == NULL
    //
    // if sizeof (restore_data_buffer) != 4rd param, then it is seg fault.
    // it's a user level bug... so I don't care of it.
    if (-1 == cubrid_restore_write (restore_handle, restore_info.backup_level, NULL, 4096))
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
    /* test - invalid restore arguments */
    pass_invalid_restore_info ();

    pass_null_restore_handle ();

    pass_invalid_restore_handle ();

    pass_invalid_restore_args ();

    return 0;
}
