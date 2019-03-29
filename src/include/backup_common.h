#ifndef _BACKUP_COMMON_H_
#define _BACKUP_COMMON_H_

#include <unistd.h>

#define SUCCESS_FRAGMENTED (1) // cubrid_backup_read () 시 읽을 데이터가 남아 있음을 의미
#define SUCCESS            (0)
#define FAILURE            (-1)

#define IS_SUCCESS(a) ((a) == SUCCESS)
#define IS_FAILURE(a) ((a) != SUCCESS)

#define IS_NULL(a) ((a) == NULL)

#define IS_ZERO(a) ((a) == 0)
#define IS_NOT_ZERO(a) ((a) != 0)

#define PATH_MAX 4096

typedef enum
{
    false,
    true
} bool;

#endif
