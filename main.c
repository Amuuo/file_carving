#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include <locale.h>
#include <signal.h>

typedef __uint8_t byte;


typedef struct Block_info {
    long long* headers;
    byte** info;        
    int header_count;
} Block_info;



typedef struct Cmd_Options {
    int        argc;
    char**     argv;
    int        verbose;
    int        blocksize;
    long long  offset_start;
    long long  offset_end;
    int        searchsize;
    char*      output_folder;
    char*      input;
    char*      output_file;
} Cmd_Options;



void resize_block_info(Block_info*, int, Cmd_Options*);
void init_block_memory(Block_info*, int);
void add_header_info(Block_info*, byte*, long long, int);    
void open_io_files(FILE**, FILE**, Cmd_Options*);
void get_cmd_options();
void print_headers_to_file(Cmd_Options*, Block_info*, FILE**);
void sig_handler(int signal_number);



int terminate_early;







/*======================
           MAIN
 ======================*/
int main(int argc, char** argv)
{
    terminate_early = 0;
    signal(SIGINT, sig_handler);

    Block_info block_info;
    Cmd_Options cmds = {argc, argv, 0, 32768, 0, 0, 128, NULL, NULL};
    
    get_cmd_options(&cmds);
    init_block_memory(&block_info, cmds.searchsize);
    setlocale(LC_NUMERIC, "");

    FILE*        input_file;
    FILE*        output_file;
    FILE*        header_file;                  
    regex_t      regex;
    int          regex_i;            
    const char*  regex_str = "^II.{6}CR";        
    long long    i;
    int          j;
    



    byte* transfer = (byte*)malloc(cmds.searchsize);    
    if (cmds.verbose){
        printf("\n>> Transfer alloc... %'d bytes", cmds.searchsize);
    }
    
    byte* buffer = (byte*)malloc(cmds.blocksize);
    if(cmds.verbose){
        printf("\n>> Buffer alloc... %'d bytes", cmds.blocksize);
    }    
    cmds.output_file = (char*)malloc(50);


    open_io_files(&input_file, &output_file, &cmds);    
        
    regex_i = regcomp(&regex, regex_str, REG_EXTENDED);    
    
    
    for (i = cmds.offset_start; i < cmds.offset_end; i+=cmds.blocksize ) {
                        
        fseek(input_file, i, 0);        
        
        /*
        if ((cmds.offset_end - i) < cmds.blocksize) {
            cmds.blocksize  = cmds.offset_end - i;
            buffer = (byte*)realloc(buffer, cmds.blocksize);
        }
        */
                
        if ((fread(buffer, 1, cmds.blocksize, input_file)) < 0) {            
            fprintf(stderr, "fread failed..");
            exit(1);
        }            
        
        if(cmds.verbose){
            if ((i % 1000000) < 300 )
                printf("\nBlock %'lld", i);
        }

        for (j = 0; j < cmds.blocksize; j+=cmds.searchsize) {            
            memcpy(transfer, &buffer[j] , cmds.searchsize);
            regex_i = regexec(&regex, (const char*)transfer, 0, NULL, 0);
            
            if(!regex_i) {
                printf("\nMATCH!");
                fflush(stdout);
                resize_block_info(&block_info, cmds.searchsize, &cmds);
                add_header_info(&block_info, transfer, i, cmds.searchsize);
            }
            if (terminate_early)
                goto cleanup;

        }
        //memset(transfer, 0, cmds.searchsize);
        //memset(buffer, 0, cmds.blocksize);
    }
    cleanup:

    print_headers_to_file(&cmds, &block_info, &header_file);
            
    fclose(input_file);
    fclose(output_file);
    free(buffer);
}





void resize_block_info(Block_info* block, int searchsize, Cmd_Options* cmds) {    
    int count = block->header_count + 1;        
    
    if (cmds->verbose) {
        printf("\nblock_info.headers realloc: %'d", 8*count);
        printf("\nblock_info.info realloc: %'d", count*8);    
        printf("\nblock_info.info[%d] malloc: %'d", count-1, searchsize);    
        fflush(stdout);
    }
    
    block->headers = (long long*)realloc(block->headers, 8*count);                            
    
    block->info = (byte**)realloc(block->info, 8*count);    
    
    block->info[count-1] = (byte*)malloc(searchsize);
    
        
}






void add_header_info(Block_info* block, byte* transfer_info, 
                     long long block_offset, int searchsize) {        
    memcpy(block->info[block->header_count], transfer_info, searchsize);          
    block->headers[block->header_count++] = block_offset;
}






void init_block_memory(Block_info* block, int searchsize) {
    block->header_count = 0;        
    block->headers = (long long*)malloc(8);                            
    block->info = (byte**)malloc(8);
    block->info[0] = (byte*)malloc(searchsize);           

}




void open_io_files(FILE** input_file, FILE** output_file, Cmd_Options* cmds) {        
                
    if (!(*input_file = fopen(cmds->input, "rb"))) {
        fprintf(stderr, "\n>> Input file failed to open");
        exit(1);
    }    
    printf("\n>> Input file opened: %s", cmds->input);        
               
    
    strcat(cmds->output_file, cmds->output_folder);
    strcat(cmds->output_file, "output.txt");    
    if (!(*output_file = fopen(cmds->output_file, "wb+"))) {
            fprintf(stderr, "\n>> Output file failed to open");
            exit(1);
    }    
    printf("\n>> Output file opened: %s", cmds->output_file);
    fflush(stdout);
    
}





void print_headers_to_file(Cmd_Options* cmds, Block_info* block, FILE** header_file) {
    
    char file_str[50];
    byte line[16];
    int i, j, k;    
    
    strcat(file_str, cmds->output_folder);
    strcat(file_str, "headers.txt");
    printf("\nHeader File: %s", file_str);    

    
    if (!(*header_file = fopen(file_str, "w+"))) {
        fprintf(stderr, "\n>> Output file failed to open");
        exit(1);
    }    
    
    if(cmds->verbose)
        printf("\n>> Header file opened: %s", file_str);                 
               
    for (i = 0; i < block->header_count-1; ++i) {
        
        for (j = 0; j < cmds->searchsize; j+=16) {                        
            memcpy(line, (block->info[i])+j, 16);            
            fprintf(*header_file, "\n%lld  ", block->headers[i]);
            
            for( k = 0; k < 16; ++k) {
                fprintf(*header_file, "%02x ", line[k]);
                if (k == 7)
                    fprintf(*header_file, " ");                
            }            
            fprintf(*header_file, " |");
            for( k = 0; k < 16; ++k) {                
                fprintf(*header_file, "%c", 
                         (line[k] > 33 && line[k] < 127) ? line[k] : '.');                
            }
            fprintf(*header_file, "|");
        }
        fprintf(*header_file, "\n\n");
    }    
    fclose(*header_file);
}






void get_cmd_options(Cmd_Options* cmds)
{
    setlocale(LC_NUMERIC, "");

    static struct option long_options[] =
    {
        {"blocksize",     required_argument, NULL, 'b'},
        {"offset_start",  required_argument, NULL, 'S'},
        {"offset_end",    required_argument, NULL, 'E'},
        {"searchsize",    required_argument, NULL, 's'},
        {"input",         required_argument, NULL, 'i'},
        {"output_folder", required_argument, NULL, 'o'},
        {"searchsize",    required_argument, NULL, 's'},
        {"verbose",       no_argument,       NULL, 'v'}, 
        {"help",          no_argument,       NULL, 'h'},                
        {NULL, 0, NULL, 0}
    };    

    extern char *optarg;
    extern int optind;  
    int cmd_option;


    while ((cmd_option = getopt_long(cmds->argc, cmds->argv,
                                     "b:S:E:s:i:o:vh", long_options, NULL)) != -1) {
    
        switch (cmd_option) 
        {
            
        case 'b':
            cmds->blocksize = atoi(optarg);                
            break;
        
        case 'S':
            cmds->offset_start = atoll(optarg);
            break;
        
        case 'E':
            cmds->offset_end = atoll(optarg);
            break;

        case 's':
            cmds->searchsize = atoi(optarg);
            break;
        
        case 'i':
            cmds->input = (char*)malloc(sizeof(char)*30);                
            strcpy(cmds->input, optarg);                    
            break;

        case 'o':
            cmds->output_folder = (char*)malloc(sizeof(char)*30);                       
            strcpy(cmds->output_folder, optarg);
            break;
        
        case 'v':
            cmds->verbose = 1;
            break;

        case 'h':
            printf("\n\t--blocksize     -b   <size of logical blocks in bytes>");
            printf("\n\t--offset_start  -S  <offset from start of disk in bytes>");
            printf("\n\t--offset_end    -E  <offset from start of disk in bytes>");
            printf("\n\t--searchsize    -s   <size of chunk to search for input disk in bytes>");
            printf("\n\t--input         -i   <file to use for header search>");
            printf("\n\t--output_folder -o   <folder to use for output files>");
            printf("\n\t--verbose       -v   <display verbose output of ongoing process>");
            printf("\n\t--help          -h   <print help options>\n\n");
        
            exit(0);
            
            break;
        
        
        default: 
            break;
        
        }
    }

    if(!cmds->output_folder) {
        
        cmds->output_folder = (char*)malloc(sizeof(char)*50);
        memset(cmds->output_folder, 0, 50);  
        
        if(!(getcwd(cmds->output_folder, 50))){
            printf("\nCould not retrieve directory with 'getcwd'");
        }
        if(cmds->verbose) {
            strcat(cmds->output_folder, "/");
            printf("\n>> Setting current directory for output: %s", 
                   cmds->output_folder);
        }
    }

    if(cmds->offset_end == 0) {
        FILE* file_size_stream = fopen(cmds->input, "r");
        fseek(file_size_stream, 0, SEEK_END);
        cmds->offset_end = ftell(file_size_stream);
        fclose(file_size_stream);        
    }

    long long filesize = cmds->offset_end - cmds->offset_start;
    if (filesize < cmds->blocksize) 
        cmds->blocksize = filesize;
    
    if(cmds->verbose) {
                
        printf("\n>> Offset Start: %'lld bytes", cmds->offset_start);
        printf("\n>> No offset end detected, setting offset to end of file");
        printf("\n>> Offset End: %'lld bytes", cmds->offset_end);
        printf("\n>> Input file: %s", cmds->input);                 
        printf("\n>> Input file size: %'lld bytes", filesize);
        printf("\n>> Blocksize: %'d bytes", cmds->blocksize);        
        printf("\n>> Search Size: %'d bytes", cmds->searchsize);
        printf("\n>> Output folder: %s", cmds->output_folder);
    }

    
    if (!cmds->input) {
        printf("\n>> Input file required");
        exit(1);
    }
}



void sig_handler(int signal_number) {

    terminate_early = 1;

}