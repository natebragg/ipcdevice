/*
 * A set of simple tests designed to exercise the ipcdevice module
 * Build, run, inspect.
 */
#include <stdio.h>
#include <string.h>

#define ASSERT_THAT( expr ) do{ if (!(expr)) { printf("ASSERT FAILED: " # expr "\n" ); return 1; } }while(0)

int test_write()
{
    FILE *ipc = NULL;
    const char *message = "shmowzow!";
    int len, result;

    len = strnlen(message, 20)+1;

    ipc = fopen("/dev/ipcdevice","w");
    ASSERT_THAT( ipc != NULL );
    result = fwrite(message, sizeof(char), len, ipc);
    ASSERT_THAT( result == len );
    return 0;
}

int main(int argv, char **argc){
    int result = 0;
    result += test_write();
    return result;
}
