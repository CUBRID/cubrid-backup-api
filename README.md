## Overview
* This is C interfaces to interwork with 3rd party backup solutions
* This consists of a header file and a shared library. The name of the files are as following:
  * cubrid_backup_api.h
  * libcubridbackupapi.so
## Structures
* CUBRID_BACKUP_INFO
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
|sa_mode|Whether to perform backup in stand-alone mode</br>&nbsp;&nbsp;0 - client / server mode (default)</br>&nbsp;&nbsp;1 - stand-alone mode|
|no_check| |
|compress| |
|db_name| |
   
