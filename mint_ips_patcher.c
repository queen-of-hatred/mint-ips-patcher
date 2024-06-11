#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//Patch and EOF minimum
#define IPS_MIN_SIZE 8
#define IPS_MAX_SIZE 16777216

#define ROM_SIZE_MAX 536870912

#define TRIPLE_BYTESWAP(input_buffer) \
    ((((uint) input_buffer[0]) << 16 ) & 0x00FF0000 | \
    (((uint) input_buffer[1]) << 8 ) & 0x0000FF00 | \
    (((uint) input_buffer[2])) & 0x000000FF)
    
#define DOUBLE_BYTESWAP(input_buffer) \
    ((((uint) input_buffer[0]) << 8 ) & 0x0000FF00 | \
    (((uint) input_buffer[1])) & 0x000000FF)

int main(int argc, char** argv){
    //Validate command line arguments
    const char* usage = "usage: ./mint-ips-patcher <rom> <patch>\n";
    if(argc < 3){
        fprintf(stderr, "%s", usage);
        exit(1);
    }

    //Open rom (game) file 
    FILE* rom_file = fopen(argv[1], "rw");
    if(!rom_file){
        fprintf(stderr, "Failed to open rom file, please check file permissions\n.");
        exit(1);
    }

    //Open ips (patch) file
    FILE* ips_file = fopen(argv[2], "r");
    if(!ips_file){
        fprintf(stderr, "Failed to open ips file, please check file permissions\n");
        fclose(rom_file);
        exit(1);
    }
    
    //Read size of input files and allocate buffer equal to their size

    //IPS File Verification
    fseek(ips_file, 0, SEEK_END);
    int ips_file_size = ftell(ips_file);
    rewind(ips_file);
    if(ips_file_size < IPS_MIN_SIZE || ips_file_size >= IPS_MAX_SIZE){
        fprintf(stderr, "IPS file must be greater than %d bytes and less than %d bytes, exiting...\n", \
                IPS_MIN_SIZE, IPS_MAX_SIZE);
        goto cleanup_file_ptrs;
    }

    //Rom File Verification
    fseek(rom_file, 0, SEEK_END);
    int rom_file_size = ftell(rom_file);
    rewind(rom_file);
    if(rom_file_size >= ROM_SIZE_MAX){
        fprintf(stderr, "Rom file must be less than %d bytes, exiting...\n", \
                ROM_SIZE_MAX);
        goto cleanup_file_ptrs;
    }

    //Ips file buffer creation
    char* ips_buffer = calloc(ips_file_size + 1, sizeof(char));
    if(!ips_buffer){
        fprintf(stderr, "Failed to allocate memory for ips_buffer, exiting...\n");
        goto cleanup_file_ptrs;
    }

    //Rom file buffer creation
    char* rom_buffer = calloc(rom_file_size + 1, sizeof(char));
    if(!rom_buffer){
        free(ips_buffer);
        fprintf(stderr, "Failed to allocate memory for rom_buffer, exiting...\n");
        goto cleanup_file_ptrs;
    }

    //Read input files into buffers
    fread(ips_buffer, sizeof(char), ips_file_size, ips_file);
    fread(rom_buffer, sizeof(char), rom_file_size, rom_file);
    rewind(ips_file);
    fseek(ips_file, 5, 0);

    if(strncmp(ips_buffer, "PATCH", 5) != 0){
        fprintf(stderr, "Invalid ips file: PATCH not found in first 5 bytes. Exiting...\n");
        goto cleanup_buffers;
    }

    char working_buffer[3] = {0};
    uint ips_offset[3] = {0};
    int index = 5;

    if(index + 3 >= ips_file_size){
        goto cleanup_buffers;
    }

    memcpy(working_buffer, &ips_buffer[index], 3);
    


    while((strncmp((working_buffer), "EOF", 3) != 0)){
        // Read in offset and convert from big endian 
        uint offset = TRIPLE_BYTESWAP(working_buffer);
        index += 3;
        // Read in size and convert from small 
        if(index + 2 >= ips_file_size){
            printf("Invalid IPS file, exiting....\n");
            goto cleanup_buffers;
        }

        //Read the size next. If the size is equal to 0, then it is encoded with RLE
        memcpy(working_buffer, &ips_buffer[index], 2);
        uint size = DOUBLE_BYTESWAP(working_buffer);
        index += 2;


        //We assume run length encoding
        if(size == 0){
            if(index + 2 >= ips_file_size){
                printf("Invalid IPS file, exiting....\n");
                goto cleanup_buffers;
            }

            //Read the RLE size, this is how many of the value in the next
            //byte we have compressed and need to unpack
            memcpy(working_buffer, &ips_buffer[index], 2);
            uint rle_size = DOUBLE_BYTESWAP(working_buffer);
            index += 2;

            if(index + 1 >= ips_file_size){
                printf("Invalid IPS file, exiting....\n");
                goto cleanup_buffers;
            }

            //The size we read is the number of bytes to write into the
            //rom file from the offset we read earlier, we use the value
            //located at the index as per the IPS file specification
            memcpy(&rom_buffer[offset], &ips_buffer[index], rle_size);
            index++;
        }
        else{
            //We check if we can read that many bytes ahead
            if(index + size > ips_file_size){
                printf("Invalid IPS file, exiting....\n");
                goto cleanup_buffers;
            }
            //The size we read is the number of bytes to write into the
            //rom file from the offset we read earlier.
            memcpy(&rom_buffer[offset], &ips_buffer[index], size);
            index += size;
        }
        //Read next three bytes for the offset, start of new IPS record
        memcpy(working_buffer, &ips_buffer[index], 3);
    }

    FILE* output_file = fopen("output", "w");
    fwrite(rom_buffer, sizeof(char), rom_file_size, output_file);


    cleanup_buffers:
        free(rom_buffer);
        free(ips_buffer);

    cleanup_file_ptrs:
        fclose(rom_file);
        fclose(ips_file);
        fclose(output_file);

    exit(0);
}