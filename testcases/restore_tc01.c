#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cubrid_backup_api.h"

void usage ()
{
    printf ("./restore_tc01 [DB_NAME] [BACKUP_LEVEL] [BACKUP_FILE_PATH] [RESTORE_TYPE] [RESTORE_PATH]\n\n");
    printf ("ex)\n");
    printf ("restore (full)    ==> ./restore_tc01 demodb 0 ./backup_dir/demodb_bk0v000 0 ./restore_dir\n");
    printf ("        (level 1) ==> ./restore_tc01 demodb 1 ./backup_dir/demodb_bk1v000 0 ./restore_dir\n");
    printf ("        (level 2) ==> ./restore_tc01 demodb 2 ./backup_dir/demodb_bk2v000 0 ./restore_dir\n");
}

void set_restore_info (CUBRID_RESTORE_INFO *restore_info, char *db_name, char *backup_level, char *restore_type, char *restore_path)
{
    restore_info->db_name          = db_name;
    restore_info->backup_level     = atoi (backup_level);

    if (!strcmp (restore_type, "0"))
    {
        restore_info->restore_type = RESTORE_TO_FILE;
    }

    restore_info->backup_file_path = restore_path;
}

int main (int argc, char *argv[])
{
    CUBRID_RESTORE_INFO cub_restore_info;
    void *cub_restore_handle = NULL;

    char restore_data_buffer[4096];
    int  restore_data_size = 0;

    int  total_restore_data_size = 0;

    FILE *backup_fp;

    if (argc != 6)
    {
        usage ();
        exit (1);
    }

    set_restore_info (&cub_restore_info, argv[1], argv[2], argv[4], argv[5]);

    backup_fp = fopen (argv[3], "r");
    if (backup_fp == NULL)
    {
        printf ("[NOK] failed to open backup file\n");
        exit (1);
    }

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] failed the execution of cubrid_backup_initialize ()\n");
        exit (1);
    }

    if (-1 == cubrid_restore_begin (&cub_restore_info, &cub_restore_handle))
    {
        printf ("[NOK] failed the execution of cubrid_restore_begin ()\n");
        exit (1);
    }

    while (1)
    {
        restore_data_size = fread (restore_data_buffer, 1, 4096, backup_fp);

        if (restore_data_size != 0)
        {
            if (-1 == cubrid_restore_write (cub_restore_handle, cub_restore_info.backup_level, restore_data_buffer, restore_data_size))
            {
                printf ("[NOK] failed the execution of cubrid_restore_write ()\n");
                exit (1);
            }

            total_restore_data_size += restore_data_size;
        }
        else
        {
            break;
        }
    }

    if ( 0 == total_restore_data_size )
    {
        printf ("[NOK] restore_data_size ==> %d\n", total_restore_data_size);
    }
    else
    {
        printf ("[OK] restore_data_size ==> %d\n", total_restore_data_size);
    }

    fclose (backup_fp);

    if (-1 == cubrid_restore_end (cub_restore_handle))
    {
        printf ("[NOK] failed the execution of cubrid_restore_end ()\n");
        exit (1);
    }

    if (-1 == cubrid_backup_finalize ())
    {
        printf ("[NOK] failed the execution of cubrid_backup_finalize ()\n");
        exit (1);
    }

    return 0;
}
