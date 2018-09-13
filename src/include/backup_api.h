#ifndef _CUBRID_BACKUP_API_H_
#define _CUBRID_BACKUP_API_H_

typedef struct cubrid_backup_info CUBRID_BACKUP_INFO;
struct cubrid_backup_info
{
    int backup_level;
    const char* db_name;
};

typedef enum restore_type RESTORE_TYPE;
enum restore_type
{
    RESTORE_TO_DB,
    RESTORE_TO_FILE
};

typedef struct cubrid_restore_info CUBRID_RESTORE_INFO;
struct cubrid_restore_info
{
    RESTORE_TYPE restore_type;
    int backup_level;
    const char* up_to_date; /* format: dd-mm-yyyy:hh:mm:ss */
    const char* backup_file_path;
    const char* db_name;
};

int cubrid_backup_initialize (void);

int cubrid_backup_begin (CUBRID_BACKUP_INFO* backup_info, void** backup_handle);
int cubrid_backup_read (void* backup_handle,
                        void* buffer,
                        unsigned int buffer_size,
                        unsigned int* data_len);
int cubrid_backup_end (void* backup_handle);

int cubrid_restore_begin (CUBRID_RESTORE_INFO* restore_info, void** restore_handle);
int cubrid_restore_write (void* restore_handle,
                          int backup_level,
                          void* buffer,
                          unsigned int data_len);
int cubrid_restore_end (void* restore_handle);

int cubrid_backup_finalize (void);

#endif
