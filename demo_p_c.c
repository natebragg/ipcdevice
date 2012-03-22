/*
 * demo_p_c, a simple producer/consumer demo using the ipcdevice module.
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
#include <unistd.h>

#define PROC_NAME "demo_p_c"
#define BUF_SIZE 4096

void consumer(int msg_cnt){
    FILE *ipc = NULL;
    char *message = NULL;
    size_t bytes_read = 0, br_total = 0;

    ipc = fopen("/dev/ipcdevice","r");
    if( ipc == NULL ){
        printf( PROC_NAME ": could not open ipc for reading!\n");
        return;
    }

    message = malloc(BUF_SIZE);
    if( message == NULL ){
        printf( PROC_NAME ": could not malloc read buffer!\n");
        fclose( ipc );
        return;
    }

    do{
        printf( PROC_NAME ": read '" );
        do{
            br_total = 0;
            do{
                bytes_read = fread(message+br_total, sizeof(char), BUF_SIZE-br_total, ipc);
                br_total += bytes_read;
            }while( message[br_total-1] != 0 && br_total != BUF_SIZE );
            printf( "%.*s", br_total, message);
        }while( message[br_total-1] != 0 );
        printf( "'\n" );
        br_total = 0;
    } while( --msg_cnt > 0 );

    fclose( ipc );
    free( message );
}

void producer(int msg_cnt, char **messages){
    FILE *ipc = NULL;
    size_t len, bytes_written;

    ipc = fopen("/dev/ipcdevice","w");
    if( ipc == NULL ){
        printf( PROC_NAME ": could not open ipc for writing!\n");
        return;
    }

    while( msg_cnt-- > 0 ){
        len = strnlen(messages[0], BUF_SIZE) + 1;
        bytes_written = fwrite(messages[0], sizeof(char), len, ipc);
        if( bytes_written != len ){
            printf( PROC_NAME ": trouble writing message '%s': only wrote %d"
                " bytes.\n", messages[0], bytes_written);
        }
        messages++;
        fflush(ipc);
    }
    fclose( ipc );
}

int main(int argc, char **argv){
    int result = 0;

    if( argc == 1){
        printf("USAGE: " PROC_NAME " MESSAGE_ONE [MESSAGE_TWO ...]\n");
        return 1;
    }

    if( (result = fork()) == 0 ){
        producer(argc-1, argv+1);
    } else {
        if( result == -1 ){
            //child was not created :^(
            printf( PROC_NAME ": error - child process could not be created.\n");
            return 2;
        }
        consumer(argc-1);
    }
    return 0;
}
