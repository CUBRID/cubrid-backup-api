#include <stdio.h>
#include <stdlib.h>
#include "cubrid_backup_api.h"

void usage ()
{
    printf ("./cubrid_backup [DB_NAME] [BACKUP_LEVEL] [BACKUP_FILE_PATH]\n\n");
    printf ("ex)\n");
    printf ("backup (full)    ==> ./cubrid_backup demodb 0 ./backup_dir/demodb_bk0v000\n");
    printf ("       (level 1) ==> ./cubrid_backup demodb 1 ./backup_dir/demodb_bk1v000\n");
    printf ("       (level 2) ==> ./cubrid_backup demodb 2 ./backup_dir/demodb_bk2v000\n");
}

void set_backup_info (CUBRID_BACKUP_INFO *backup_info, char *db_name, char *backup_level)
{
    backup_info->db_name      = db_name;
    backup_info->backup_level = atoi (backup_level);
}

int main (int argc, char *argv[])
{
    CUBRID_BACKUP_INFO cub_backup_info;
    void *cub_backup_handle = NULL;

    char backup_data_buffer[4096];
    int  backup_data_size = 0;

    int  total_backup_data_size = 0;

    FILE *backup_fp;

    if (argc != 4)
    {
        usage ();
        exit (1);
    }

    set_backup_info (&cub_backup_info, argv[1], argv[2]);

    backup_fp = fopen (argv[3], "w+b");

    if (-1 == cubrid_backup_initialize ())
    {
        printf ("failed the execution of cubrid_backup_initialize ()\n");
        exit (1);
    }

    if (-1 == cubrid_backup_begin (&cub_backup_info, &cub_backup_handle))
    {
        printf ("failed the execution of cubrid_backup_begin ()\n");
        exit (1);
    }

    while (1)
    {
        if (-1 == cubrid_backup_read (cub_backup_handle, backup_data_buffer, 4096, &backup_data_size))
        {
            printf ("failed the execution of cubrid_backup_read ()\n");
            exit (1);
        }

        if (backup_data_size != 0)
        {
            fwrite (backup_data_buffer, 1, backup_data_size, backup_fp);

            total_backup_data_size += backup_data_size;
        }
        else
        {
            break;
        }
    }

    printf ("backup_data_size ==> %d\n", total_backup_data_size);

    fclose (backup_fp);

    if (-1 == cubrid_backup_end (cub_backup_handle))
    {
        printf ("failed the execution of cubrid_backup_end ()\n");
        exit (1);
    }

    if (-1 == cubrid_backup_finalize ())
    {
        printf ("failed the execution of cubrid_backup_finalize ()\n");
        exit (1);
    }

    return 0;
}
