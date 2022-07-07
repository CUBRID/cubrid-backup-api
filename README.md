## Overview
* This is C interfaces to interwork with 3rd party backup solutions
* This consists of a header file and a shared library. The name of the files are as following:
  * cubrid_backup_api.h
  * libcubridbackupapi.so
## Structures
### CUBRID_BACKUP_INFO
* This is a struct to can set backup information required to perform cubrid backup
* The target database name to backup or the backup level, etc can be set by this
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

|member variable|description|
|-|-|
|backup_level|The backup level supported by cubrid backup</br>&nbsp;&nbsp;0 - full backup</br>&nbsp;&nbsp;1 - incremental backup</br>&nbsp;&nbsp;2 - big incremental backup|
|remove_archive|Whether to remove archive log volumes that will not be used anymore in subsequent backups after the current one is complete</br>&nbsp;&nbsp;0 - do not delete (default)</br>&nbsp;&nbsp;1 - delete|
|sa_mode|Whether to perform backup in stand-alone mode</br>&nbsp;&nbsp;0 - client/server mode; on-line backup (default)</br>&nbsp;&nbsp;1 - stand-alone mode; off-line backup|
|no_check|Whether to perform a consistency check</br>&nbsp;&nbsp;0 - do consistency check (default)</br>&nbsp;&nbsp;1 - no consistency check|
|compress|Whether to perform data compression when backup</br>&nbsp;&nbsp;0 - no compression (default)</br>&nbsp;&nbsp;1 - do compression|
|db_name|target database name to backup|
### CUBRID_RESTORE_INFO
* This is a struct to can set restore information required to perform cubrid restore
* The target database name to restore or the backup level, etc can be set by this
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

|member variable|description|
|-|-|
|restore_type|Specify the restore type</br>&nbsp;&nbsp;RESTORE_TO_DB - not yet supported</br>&nbsp;&nbsp;RESTORE_TO_FILE - Create a backup volume under the path specified by the backup_file_path option|
|backup_level|The backup level of the backup volume used for restore|
|up_to_date|not yet supported|
|backup_file_path|The directory path to save the backup volume when the restore_type option is set to the RESTORE_TO_FILE|
|db_name|target database name to restore|
### RESTORE_TYPE
|type|description|
|-|-|
|RESTORE_TO_DB|not yet supported|
|RESTORE_TO_FILE|restore the backup data to a file|
## Functions
### cubrid_backup_initialize
<pre>
<code>
int cubrid_backup_initialize (void);
</code>
</pre>
#### description
API function that should be called first to use cubrid-backup-api
#### return
|value|description|
|-|-|
|0|Success|
|-1|Failure|
### cubrid_backup_finalize
<pre>
<code>
int cubrid_backup_finalize (void);
</code>
</pre>
#### description
An API function paired with the cubrid_backup_initialize() function, called to end use of cubrid-backup-api
#### return
|value|description|
|-|-|
|0|Success|
|-1|Failure|
### cubrid_backup_begin
<pre>
<code>
int cubrid_backup_begin (CUBRID_BACKUP_INFO* backup_info, void** backup_handle);
</code>
</pre>
#### description
Called to perform backup using cubrid-backup-api. When the user inputs backup information into the CUBRID_BACKUP_INFO data structure and calls it, the backup_handle is passed
#### return
|value|description|
|-|-|
|0|Success|
|-1|Failure|
#### arguments
|name|in/out|description|
|-|-|-|
|backup_info|in|Refer to the description of the CUBRID_BACKUP_INFO data structure|
|backup_handle|out|A backup handle that internally identifies a backup</br></br>Used as an input argument when calling cubrid_backup_read () and cubrid_backup_end ()|
### cubrid_backup_end
<pre>
<code>
int cubrid_backup_end (void* backup_handle);
</code>
</pre>
#### description
Called to end a backup started with cubrid_backup_begin()
#### return
|value|description|
|-|-|
|0|Success|
|-1|Failure|
#### arguments
|name|in/out|description|
|-|-|-|
|backup_handle|in|Backup handle received when calling cubrid_backup_begin()|
### cubrid_backup_read
<pre>
<code>
int cubrid_backup_read (void* backup_handle, void* buffer, unsigned int buffer_size, unsigned int* data_len);
</code>
</pre>
#### description
Call to read backup data through API
#### return
|value|description|
|-|-|
|1|Success - Need to read the rest of the backup data by calling cubrid_backup_read() again|
|0|Success - Backup complete|
|-1|Failure|
#### arguments
|name|in/out|description|
|-|-|-|
|backup_handle|in|Backup handle received when calling cubrid_backup_begin()|
|buffer|out|User buffer to hold backup data|
|buffer_size|in|buffer size|
|data_len|out|Actual data size copied to buffer|
### cubrid_restore_begin
<pre>
<code>
int cubrid_restore_begin (CUBRID_RESTORE_INFO* restore_info, void** restore_handle);
</code>
</pre>
#### description
Called to restore backed up data to DB or file
#### return
|value|description|
|-|-|
|0|Success|
|-1|Failure|
#### arguments
|name|in/out|description|
|-|-|-|
|restore_info|in|Refer to the description of the CUBRID_RESTORE_INFO data structure|
|restore_handle|out|restore handle that internally identifies restore</br></br>Used as an input argument when calling cubrid_restore_write () and cubrid_restore_end ()|
### cubrid_restore_end
<pre>
<code>
int cubrid_restore_end (void* restore_handle);
</code>
</pre>
#### description
Called to end the recovery started with cubrid_restore_begin()
#### return
|value|description|
|-|-|
|0|Success|
|-1|Failure|
#### arguments
|name|in/out|description|
|-|-|-|
|restore_handle|in|The restore handle received when calling cubrid_restore_begin()|
### cubrid_restore_write
<pre>
<code>
int cubrid_restore_write (void* restore_handle, int backup_level, void* buffer, unsigned int data_len);
</code>
</pre>
#### description
Passing backed up data to API for database restore
#### return
|value|description|
|-|-|
|0|Success|
|-1|Failure|
#### arguments
|name|in/out|description|
|-|-|-|
|restore_handle|in|The restore handle received when calling cubrid_restore_begin()|
|backup_level|in|The backup level of the backup data it carries|
|buffer|in|Buffer with backup data|
|data_len|in|Actual data size in buffer|
## API function call transition diagram
### backup
### restore
## API test sample code
### backup
#### sample code
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
#### build
<pre>
<code>
gcc -o backup_sample -L. -lcubridbackupapi -lpthread backup_sample.c
</code>
</pre>
### restore
#### sample code
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
#### build
<pre>
<code>
gcc -o restore_sample -L. -lcubridbackupapi -lpthread restore_sample.c
</code>
</pre>
