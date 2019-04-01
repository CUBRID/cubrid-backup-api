#include <stdio.h>
#include <stdlib.h>
#include "cubrid_backup_api.h"

CUBRID_RESTORE_INFO cub_restore_info;
void *cub_restore_handle = NULL;

void usage ()
{
    printf ("./restore_tc02 [DB_NAME] [BACKUP_LEVEL] [RESTORE_TYPE] [RESTORE_PATH]\n\n");
    printf ("ex)\n");
    printf ("restore (full)    ==> ./restore_tc02 demodb 0 0 ./restore_dir\n");
    printf ("        (level 1) ==> ./restore_tc02 demodb 1 0 ./restore_dir\n");
    printf ("        (level 2) ==> ./restore_tc02 demodb 2 0 ./restore_dir\n");
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

void call_cubrid_restore_begin_without_initialize (void)
{
    if (-1 == cubrid_restore_begin (&cub_restore_info, &cub_restore_handle))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }
}

void call_cubrid_restore_write_without_begin (void)
{
    char restore_data_buffer[4096];

    if (-1 == cubrid_restore_write (cub_restore_handle, cub_restore_info.backup_level, restore_data_buffer, 4096))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }
}

void call_cubrid_restore_end_without_begin (void)
{
    if (-1 == cubrid_restore_end (cub_restore_handle))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }
}

void call_cubrid_restore_begin_x_2 (void)
{
    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_restore_begin (&cub_restore_info, &cub_restore_handle))
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_restore_begin (&cub_restore_info, &cub_restore_handle))
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

void call_cubrid_restore_end_x_2 (void)
{
    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_restore_begin (&cub_restore_info, &cub_restore_handle))
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_restore_end (cub_restore_handle))
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_restore_end (cub_restore_handle))
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
    if (argc != 5)
    {
        usage ();
        exit (1);
    }

    set_restore_info (&cub_restore_info, argv[1], argv[2], argv[3], argv[4]);

    /* test - API call sequence checking */
    call_cubrid_restore_begin_without_initialize ();

    call_cubrid_restore_write_without_begin ();

    call_cubrid_restore_end_without_begin ();

    /* test - API call again: MUST error */
    call_cubrid_restore_begin_x_2 ();

    call_cubrid_restore_end_x_2 ();

    return 0;
}
