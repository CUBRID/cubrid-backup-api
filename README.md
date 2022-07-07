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
