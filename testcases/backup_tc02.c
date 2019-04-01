#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "cubrid_backup_api.h"

CUBRID_BACKUP_INFO cub_backup_info;
void *cub_backup_handle = NULL;
void *cub_backup_handle2 = NULL;

void usage ()
{
    printf ("./backup_tc02 [DB_NAME] [BACKUP_LEVEL]\n\n");
    printf ("ex)\n");
    printf ("backup (full)    ==> ./backup_tc02 demodb 0\n");
    printf ("       (level 1) ==> ./backup_tc02 demodb 1\n");
    printf ("       (level 2) ==> ./backup_tc02 demodb 2\n");
}

void set_backup_info (CUBRID_BACKUP_INFO *backup_info, char *db_name, char *backup_level)
{
    backup_info->db_name      = db_name;
    backup_info->backup_level = atoi (backup_level);
}

void call_cubrid_backup_begin_without_initialize (void)
{
    if (-1 == cubrid_backup_begin (&cub_backup_info, &cub_backup_handle))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }
}

void call_cubrid_backup_read_without_begin (void)
{
    char backup_data_buffer[4096];
    int  backup_data_size = 0;

    if (-1 == cubrid_backup_read (cub_backup_handle, backup_data_buffer, 4096, &backup_data_size))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }
}

void call_cubrid_backup_end_without_begin (void)
{
    if (-1 == cubrid_backup_end (cub_backup_handle))
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }
}

void call_cubrid_backup_finalize_without_initialize (void)
{
    if (-1 == cubrid_backup_finalize ())
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }
}

void call_cubrid_backup_initialize_x_2 (void)
{
    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_backup_initialize ())
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

void call_cubrid_backup_begin_x_2 (void)
{
    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_backup_begin (&cub_backup_info, &cub_backup_handle))
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_backup_begin (&cub_backup_info, &cub_backup_handle2))
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

void call_cubrid_backup_end_x_2 (void)
{
    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_backup_begin (&cub_backup_info, &cub_backup_handle))
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_backup_end (cub_backup_handle))
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_backup_end (cub_backup_handle))
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

void call_cubrid_backup_finalize_x_2 (void)
{
    if (-1 == cubrid_backup_initialize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_backup_finalize ())
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }

    if (-1 == cubrid_backup_finalize ())
    {
        printf ("[OK] %s\n", __func__);
    }
    else
    {
        printf ("[NOK] %s\n", __func__);
        exit (1);
    }
}

int main (int argc, char *argv[])
{
    if (argc != 3)
    {
        usage ();
        exit (1);
    }

    set_backup_info (&cub_backup_info, argv[1], argv[2]);

    /* test - API call sequence checking */
    call_cubrid_backup_begin_without_initialize ();

    call_cubrid_backup_read_without_begin ();

    call_cubrid_backup_end_without_begin ();

    call_cubrid_backup_finalize_without_initialize ();

    /* test - API call again: MUST error */
    call_cubrid_backup_initialize_x_2 ();
    
    call_cubrid_backup_begin_x_2 ();
    sleep(1);
    call_cubrid_backup_end_x_2 ();

    call_cubrid_backup_finalize_x_2 ();

    return 0;
}
