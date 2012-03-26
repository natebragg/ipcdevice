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

#include "ipcdevice.h"

#define PROC_NAME "demo_p_c"
#define BUF_SIZE 4096

int rot = 0;
int reverse = 0;
int base64 = 0;
char * filename = 0;

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
            }while( !feof(ipc) && br_total != BUF_SIZE );
            printf( "%.*s", br_total, message);
        }while( !feof(ipc) );
        printf( "'\n" );
        br_total = 0;
    } while( --msg_cnt > 0 );

    fclose( ipc );
    free( message );
}

void producer(int msg_cnt, char **messages){
    FILE *ipc = NULL;
    FILE *corpus = NULL;
    char *corpus_body = NULL;
    size_t len, bytes_written;
    size_t corpus_length = 0;

    ipc = fopen("/dev/ipcdevice","w");
    if( ipc == NULL ){
        printf( PROC_NAME ": could not open ipc for writing!\n");
        return;
    }

	if(reverse)
		ioctl(fileno(ipc), IPC_IOC_REVERSE, 1);
	if(rot)
		ioctl(fileno(ipc), IPC_IOC_ROT13, 1);
	if(base64)
		ioctl(fileno(ipc), IPC_IOC_BASE64, 1);

	if(filename){
		corpus = fopen(filename, "r");
		fseek(corpus, 0L, SEEK_END);
		corpus_length = ftell(corpus);
		rewind(corpus);
        corpus_body = malloc(corpus_length);
        len = fread(corpus_body, sizeof(char), corpus_length, corpus);
		setvbuf(ipc, NULL, _IONBF, 0);
        bytes_written = fwrite(corpus_body, sizeof(char), len, ipc);
        if( bytes_written != len ){
            printf( PROC_NAME ": trouble writing message '%.*s': only wrote %d"
                " bytes.\n", len, messages[0], bytes_written);
        }
        fflush(ipc);
        free(corpus_body);
	}
    while( !filename && msg_cnt-- > 0 ){
        len = strnlen(messages[0], BUF_SIZE);
        bytes_written = fwrite(messages[0], sizeof(char), len, ipc);
        if( bytes_written != len ){
            printf( PROC_NAME ": trouble writing message '%.*s': only wrote %d"
                " bytes.\n", len, messages[0], bytes_written);
        }
        messages++;
        fflush(ipc);
    }

	if(reverse)
		ioctl(fileno(ipc), IPC_IOC_REVERSE, 0);
	if(rot)
		ioctl(fileno(ipc), IPC_IOC_ROT13, 0);
	if(base64)
		ioctl(fileno(ipc), IPC_IOC_BASE64, 0);

    fclose( ipc );
}

int main(int argc, char **argv){
    int result = 0;

    if( argc == 1){
        printf("USAGE: " PROC_NAME " [-13] [-64] [-r] <[-f filename]|MESSAGE_ONE [MESSAGE_TWO ...]>\n");
        return 1;
    }
	argc--;
	argv++;
	for(;argc;argc--, argv++){
		if( !strncmp(argv[0],"-13",3) ){
			rot = 1;
		} else if( !strncmp(argv[0],"-64",3) ){
			base64 = 1;
		} else if( !strncmp(argv[0],"-r",3) ){
			reverse = 1;
		} else if( !strncmp(argv[0],"-f",2) ){
			if( argc == 1 )
				printf( PROC_NAME ": -f must be followed by filename\n" );
			filename = argv[1];
			argc--;
			argv++;
		} else {
			break;
		}
	}

    if( (result = fork()) == 0 ){
        producer(argc, argv);
    } else {
        if( result == -1 ){
            //child was not created :^(
            printf( PROC_NAME ": error - child process could not be created.\n");
            return 2;
        }
        consumer(argc);
    }
    return 0;
}
