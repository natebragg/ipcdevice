/*
 * A set of simple tests designed to exercise the ipcdevice module
 * Copyright (C) 2012  Nate Bragg
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ASSERT_EQ( p1, p2 ) do{ if ((p1) != (p2)) { printf("ASSERT FAILED(%d): " # p1 " does not equal " # p2 "\n\t" # p1 " = %d\n\t" # p2 " = %d\n", __LINE__, (int)p1, (int)p2 ); result += 1; } }while(0)
#define ASSERT_NEQ( p1, p2 ) do{ if ((p1) == (p2)) { printf("ASSERT FAILED(%d): " # p1 " equals " # p2 "\n\t" # p1 " = %d\n\t" # p2 " = %d\n", __LINE__, (int)p1, (int)p2 ); result += 1; } }while(0)
#define ASSERT_STR_EQ( p1, p2, bsize ) do{ if ( strncmp(p1, p2, bsize) ) { printf("ASSERT FAILED(%d): " # p1 " != " # p2 "\n\t" # p1 " = %.*s\n\t" # p2 " = %.*s\n", __LINE__, bsize, p1, bsize, p2 ); result += 1; } }while(0)

int test_multi_read()
{
    FILE *ipc = NULL;
    char *message, *m_cursor;
    const char *expected = "shmowzow!";
    size_t buf_size, bytes_read;
    size_t total_bytes_read = 0;
    int result = 0;

    buf_size = 20;
    m_cursor = message = (char*)malloc(buf_size);
    memset(message, 0, buf_size);

    ipc = fopen("/dev/ipcdevice","r");
    ASSERT_NEQ( ipc, NULL );

    while( (bytes_read = fread(m_cursor, sizeof(char), 1, ipc)) == 1){
        total_bytes_read += bytes_read;
        m_cursor += bytes_read;
    }
    
    ASSERT_EQ( strnlen(expected, buf_size) + 1, total_bytes_read );
    ASSERT_STR_EQ( message, expected, buf_size );
    fclose( ipc );
    free( message );
    return result;
}

int test_single_read()
{
    FILE *ipc = NULL;
    char *message;
    const char *expected = "shmowzow!";
    size_t buf_size, bytes_read;
    int result = 0;

    buf_size = 20;
    message = (char*)malloc(buf_size);
    memset(message, 0, buf_size);

    ipc = fopen("/dev/ipcdevice","r");
    ASSERT_NEQ( ipc, NULL );
    bytes_read = fread(message, sizeof(char), buf_size, ipc);
    ASSERT_EQ( strnlen(expected, buf_size) + 1, bytes_read );
    ASSERT_STR_EQ( message, expected, buf_size );
    fclose( ipc );
    free( message );
    return result;
}

int test_write()
{
    FILE *ipc = NULL;
    const char *message = "shmowzow!";
    size_t len, bytes_written;
    int result = 0;

    len = strnlen(message, 20)+1;

    ipc = fopen("/dev/ipcdevice","w");
    ASSERT_NEQ( ipc, NULL );
    bytes_written = fwrite(message, sizeof(char), len, ipc);
    ASSERT_EQ( bytes_written, len );
    fclose( ipc );
    return result;
}

int main(int argv, char **argc){
    int result = 0;
    result += test_write();
    result += test_single_read();
    result += test_write();
    result += test_multi_read();
    return result;
}
