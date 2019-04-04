#include <stdio.h>
#include <stdlib.h>
#include "cubrid_backup_api.h"

void usage ()
{
    printf ("./backup_tc04 [DB_NAME] [BACKUP_LEVEL] [REMOVE_ARCHIVE] [SA_MODE] [NO_CHECK] [COMPRESS] [BACKUP_FILE_PATH]\n\n");
    printf ("ex)\n");
    printf ("backup (full)    ==> ./backup_tc04 demodb 0 1 0 1 1 ./backup_dir/demodb_bk0v000\n");
    printf ("       (level 1) ==> ./backup_tc04 demodb 1 1 0 1 1 ./backup_dir/demodb_bk1v000\n");
    printf ("       (level 2) ==> ./backup_tc04 demodb 2 1 0 1 1 ./backup_dir/demodb_bk2v000\n");
}

void set_backup_info (CUBRID_BACKUP_INFO *backup_info, char *db_name, char *backup_level, char *remove_archive, char *sa_mode, char *no_check, char *compress)
{
    backup_info->backup_level   = atoi (backup_level);
    backup_info->remove_archive = atoi (remove_archive);
    backup_info->sa_mode        = atoi (sa_mode);
    backup_info->no_check       = atoi (no_check);
    backup_info->compress       = atoi (compress);
    backup_info->db_name        = db_name;
}

int main (int argc, char *argv[])
{
    CUBRID_BACKUP_INFO cub_backup_info;
    void *cub_backup_handle = NULL;

    char backup_data_buffer[4096];
    int  backup_data_size = 0;

    int  total_backup_data_size = 0;

    int  backup_result;

    FILE *backup_fp;

    if (argc != 8)
    {
        usage ();
        exit (1);
    }

    set_backup_info (&cub_backup_info, argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);

    backup_fp = fopen (argv[7], "w+b");

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] failed the execution of cubrid_backup_initialize ()\n");
        exit (1);
    }

    if (-1 == cubrid_backup_begin (&cub_backup_info, &cub_backup_handle))
    {
        printf ("[NOK] failed the execution of cubrid_backup_begin ()\n");
        exit (1);
    }

    while (1)
    {
        backup_result = cubrid_backup_read (cub_backup_handle, backup_data_buffer, 4096, &backup_data_size);
        if (-1 == backup_result)
        {
            printf ("[NOK] failed the execution of cubrid_backup_read ()\n");
            exit (1);
        }

        if (backup_data_size != 0)
        {
            fwrite (backup_data_buffer, 1, backup_data_size, backup_fp);

            total_backup_data_size += backup_data_size;
        }

        if (0 == backup_result) // 0: backup end, 1: read more backup data
        {
            break;
        }
    }

    if ( 0 == total_backup_data_size )
    { 
        printf ("[NOK] backup_data_size ==> %d\n", total_backup_data_size);
    }
    else
    {
        printf ("[OK] backup_data_size ==> %d\n", total_backup_data_size);
    }

    fclose (backup_fp);

    if (-1 == cubrid_backup_end (cub_backup_handle))
    {
        printf ("[NOK] failed the execution of cubrid_backup_end ()\n");
        exit (1);
    }

    if (-1 == cubrid_backup_finalize ())
    {
        printf ("[NOK] failed the execution of cubrid_backup_finalize ()\n");
        exit (1);
    }

    return 0;
}
