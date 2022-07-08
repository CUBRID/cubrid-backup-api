## Overview
* C interfaces to interwork with 3rd party backup solutions
* a header file and a shared library are provided. The name of the files are as followings:
  * cubrid_backup_api.h
  * libcubridbackupapi.so
## Structures
### CUBRID_BACKUP_INFO
* a structure that can set required information when requesting a cubrid backup
#### Declaration in the header file
<pre>
<code>
typedef struct cubrid_backup_info CUBRID_BACKUP_INFO;
struct cubrid_backup_info
{
    int backup_level;
    int remove_archive;
    int sa_mode;
    int no_check;
    int compress;
    const char* db_name;
};
</code>
</pre>
#### Description of struct members
|member|description|
|-|-|
|backup_level|the backup level supported by cubrid</br>&nbsp;&nbsp;0 - full backup</br>&nbsp;&nbsp;1 - first incremental backup</br>&nbsp;&nbsp;2 - second incremental backup|
|remove_archive|remove archive log volumes that will no longer be used by subsequent backups after the current backup is complete</br>&nbsp;&nbsp;0 - no delete (default)</br>&nbsp;&nbsp;1 - delete|
|sa_mode|the backup execution mode supported by cubrid</br>&nbsp;&nbsp;0 - on-line backup; client/server mode (default)</br>&nbsp;&nbsp;1 - off-line backup; stand-alone mode|
|no_check|perform a consistency check to the backup data</br>&nbsp;&nbsp;0 - consistency check (default)</br>&nbsp;&nbsp;1 - no consistency check|
|compress|perform data compression to the backup data</br>&nbsp;&nbsp;0 - no compression (default)</br>&nbsp;&nbsp;1 - compression|
|db_name|target database name to backup|
### CUBRID_RESTORE_INFO
* a structure that can set required information when requesting a cubrid restore
#### Declaration in the header file
<pre>
<code>
typedef struct cubrid_restore_info CUBRID_RESTORE_INFO;
struct cubrid_restore_info
{
    RESTORE_TYPE restore_type;
    int backup_level;
    const char* up_to_date; /* format: dd-mm-yyyy:hh:mm:ss */
    const char* backup_file_path;
    const char* db_name;
};
</code>
</pre>
#### Description of struct members
|member|description|
|-|-|
|restore_type|the restore type</br>&nbsp;&nbsp;RESTORE_TO_DB - not yet supported</br>&nbsp;&nbsp;RESTORE_TO_FILE - generate a backup volume under the path specified by the backup_file_path option|
|backup_level|the backup level of the backup volume used for restore|
|up_to_date|not yet supported|
|backup_file_path|the directory path to generate a backup volume when the restore_type option is set to the RESTORE_TO_FILE|
|db_name|target database name to restore|
### RESTORE_TYPE
* an enumaration used to set the restore type in CUBRID_RESTORE_INFO structure 
#### Declaration in the header file
<pre>
<code>
typedef enum restore_type RESTORE_TYPE;
enum restore_type
{
    RESTORE_TO_DB,
    RESTORE_TO_FILE
};
</code>
</pre>
#### Description of enum members
|member|description|
|-|-|
|RESTORE_TO_DB|not yet supported|
|RESTORE_TO_FILE|restore the backup data to a file|
## Functions
### cubrid_backup_initialize()
* an API function that should be called first to use cubrid-backup-api
#### Declaration in the header file
<pre>
<code>
int cubrid_backup_initialize (void);
</code>
</pre>
#### Returns:
|return|description|
|-|-|
|0|success|
|-1|failure|
### cubrid_backup_finalize()
* an API function paired with the cubrid_backup_initialize() function. it is called to terminate the use of cubrid-backup-api
#### Declaration in the header file
<pre>
<code>
int cubrid_backup_finalize (void);
</code>
</pre>
#### Returns:
|return|description|
|-|-|
|0|success|
|-1|failure|
### cubrid_backup_begin()
* an API function called to perform backup using cubrid-backup-api
#### Declaration in the header file
<pre>
<code>
int cubrid_backup_begin (CUBRID_BACKUP_INFO* backup_info, void** backup_handle);
</code>
</pre>
#### Returns:
|return|description|
|-|-|
|0|success|
|-1|failure|
#### Parameters:
|parameter|in/out|description|
|-|-|-|
|backup_info|in|refer to the description of the CUBRID_BACKUP_INFO data structure|
|backup_handle|out|a backup handle that internally identifies a backup</br></br>used as an input argument when calling cubrid_backup_read() and cubrid_backup_end()|
### cubrid_backup_end()
* an API function called to end a backup started with cubrid_backup_begin()
#### Declaration in the header file
<pre>
<code>
int cubrid_backup_end (void* backup_handle);
</code>
</pre>
#### Returns:
|return|description|
|-|-|
|0|success|
|-1|failure|
#### Parameters:
|parameter|in/out|description|
|-|-|-|
|backup_handle|in|the backup handle received when calling cubrid_backup_begin()|
### cubrid_backup_read()
* an API function called to read backup data
#### Declaration in the header file
<pre>
<code>
int cubrid_backup_read (void* backup_handle, void* buffer, unsigned int buffer_size, unsigned int* data_len);
</code>
</pre>
#### Returns:
|return|description|
|-|-|
|1|success</br>&nbsp;- need to read the rest of the backup data by calling cubrid_backup_read() again|
|0|success</br>&nbsp;- backup complete|
|-1|failure|
#### Parameters:
|parameter|in/out|description|
|-|-|-|
|backup_handle|in|the backup handle received when calling cubrid_backup_begin()|
|buffer|out|user buffer to save the backup data|
|buffer_size|in|user buffer size|
|data_len|out|actual backup data size copied to the user buffer|
### cubrid_restore_begin()
* an API function called to restore backed up data to DB or a file
#### Declaration in the header file
<pre>
<code>
int cubrid_restore_begin (CUBRID_RESTORE_INFO* restore_info, void** restore_handle);
</code>
</pre>
#### Return:
|return|description|
|-|-|
|0|success|
|-1|failure|
#### Parameters:
|parameter|in/out|description|
|-|-|-|
|restore_info|in|refer to the description of the CUBRID_RESTORE_INFO data structure|
|restore_handle|out|a restore handle that internally identifies restore</br></br>used as an input argument when calling cubrid_restore_write() and cubrid_restore_end()|
### cubrid_restore_end()
* an API function called to terminate the restore started with cubrid_restore_begin()
#### Declaration in the header file
<pre>
<code>
int cubrid_restore_end (void* restore_handle);
</code>
</pre>
#### Returns:
|return|description|
|-|-|
|0|success|
|-1|failure|
#### Parameters:
|parameter|in/out|description|
|-|-|-|
|restore_handle|in|the restore handle received when calling cubrid_restore_begin()|
### cubrid_restore_write()
#### Declaration in the header file
* an API function passing backed up data to API for database restore
<pre>
<code>
int cubrid_restore_write (void* restore_handle, int backup_level, void* buffer, unsigned int data_len);
</code>
</pre>
#### Returns:
|return|description|
|-|-|
|0|success|
|-1|failure|
#### Parameters:
|parameter|in/out|description|
|-|-|-|
|restore_handle|in|the restore handle received when calling cubrid_restore_begin()|
|backup_level|in|the backup level of the backed up data to restore|
|buffer|in|user buffer with backup data|
|data_len|in|actual data size in the user buffer|
## API function call transition diagram
### Backup
### Restore
## Sample code
### Backup
<pre>
<code>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "cubrid_backup_api.h"
 
int main ()
{
    CUBRID_BACKUP_INFO backup_info;
    void* backup_handle;
 
    char backup_buffer[4096];
 
    unsigned int data_len;
    unsigned int total_data_len = 0;
 
    int backup_fd;
 
    int retval;
 
    backup_fd = open ("demodb_bk0v000", O_CREAT | O_WRONLY);
    if (backup_fd == -1)
    {
        printf ("[ERROR] failed to open backup file\n");
        return -1;
    }
 
    retval = cubrid_backup_initialize ();
    if (retval < 0)
    {
        printf ("[ERROR] cubrid_backup_initialize ()\n");
        return -1;
    }
 
    backup_info.backup_level = 0;
    backup_info.db_name = "demodb";
 
    retval = cubrid_backup_begin (&backup_info, &backup_handle);
    if (retval < 0)
    {
        printf ("[ERROR] cubrid_backup_begin ()\n");
        return -1;
    }
 
    while (1)
    {
        retval = cubrid_backup_read (backup_handle, backup_buffer, 4096, &data_len);
 
        if (retval < 0)
        {
            printf ("[ERROR] cubrid_backup_read ()\n");
            return -1;
        }
 
        if (data_len != 0)
        {
            write (backup_fd, backup_buffer, data_len);
 
            total_data_len += data_len;
        }
 
        if (retval == 0) // backup end
        {
            break;
        }
    }
 
    printf ("backup data size ==> %d\n", total_data_len);
 
    retval = cubrid_backup_end (backup_handle);
    if (retval < 0)
    {
        printf ("[ERROR] cubrid_backup_end ()\n");
        return -1;
    }
 
    retval = cubrid_backup_finalize ();
    if (retval < 0)
    {
        printf ("[ERROR] cubrid_backup_finalize ()\n");
        return -1;
    }
 
    close (backup_fd);
 
    return 0;
}
</code>
</pre>
<pre>
<code>
gcc -o backup_sample -L. -lcubridbackupapi -lpthread backup_sample.c
</code>
</pre>
### Restore
<pre>
<code>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "cubrid_backup_api.h"
 
int main ()
{
    CUBRID_RESTORE_INFO restore_info;
    void* restore_handle;
 
    char backup_data[4096];
 
    unsigned int data_len;
    unsigned int total_data_len = 0;
 
    int backup_fd;
 
    int retval;
 
    backup_fd = open ("demodb_bk0v000", O_RDONLY);
    if (backup_fd == -1)
    {
        printf ("[ERROR] failed to open backup file\n");
        return -1;
    }
 
    retval = mkdir ("./restore_dir", S_IRWXU);
    if (retval == -1)
    {
        printf ("[ERROR] failed to create restore directory\n");
        return -1;
    }
 
    retval = cubrid_backup_initialize ();
    if (retval < 0)
    {
        printf ("[ERROR] cubrid_backup_initialize ()\n");
        return -1;
    }
 
    restore_info.restore_type = RESTORE_TO_FILE;
    restore_info.backup_level = 0;
    restore_info.backup_file_path = "./restore_dir";
    restore_info.db_name = "demodb";
 
    retval = cubrid_restore_begin (&restore_info, &restore_handle);
    if (retval < 0)
    {
        printf ("[ERROR] cubrid_restore_begin ()\n");
        return -1;
    }
 
    do
    {
        data_len = read (backup_fd, backup_data, 4096);
        if (data_len == -1)
        {
            printf ("[ERROR] failed to read backup file\n");
            return -1;
        }
        else if (data_len == 0)
        {
            break;
        }
 
        retval = cubrid_restore_write (restore_handle, 0, backup_data, data_len);
 
        if (retval < 0)
        {
            printf ("[ERROR] cubrid_restore_write ()\n");
            return -1;
        }
 
        total_data_len += data_len;
    } while (data_len != 0);
 
    printf ("restore data size ==> %d\n", total_data_len);
 
    retval = cubrid_restore_end (restore_handle);
    if (retval < 0)
    {
        printf ("[ERROR] cubrid_restore_end ()\n");
        return -1;
    }
 
    retval = cubrid_backup_finalize ();
    if (retval < 0)
    {
        printf ("[ERROR] cubrid_backup_finalize ()\n");
        return -1;
    }
 
    close (backup_fd);
 
    return 0;
}
</code>
</pre>
<pre>
<code>
gcc -o restore_sample -L. -lcubridbackupapi -lpthread restore_sample.c
</code>
</pre>
