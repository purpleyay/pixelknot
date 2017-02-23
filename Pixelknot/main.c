//
//  main.c
//  f5+dct
//
//  Created by Gwendolyn Weston on 7/20/13.
//  Copyright (c) 2013 Gwendolyn. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "node.h"
#include "f5algorithm.h"
#include "jpeglib.h"

struct arguments {
    char *inputname;
    char *outputname;
    int embedFlag;
    int extractFlag;
    char *embedMessage;
    char *userPassword;
    size_t message_size;
};

struct coefficients {
    size_t size;
    buffer_coefficient *array;
};

static void parse_arguments(int argc, char *argv[], struct arguments *args)
{
	if (argc < 5 || argc > 7) {
        fprintf(stderr, "wrong number of arguments\n");
        exit(EXIT_FAILURE);
    }

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
        fprintf(stdout, "Current working dir: %s\n", cwd);

	args->inputname = argv[1];
	args->outputname = argv[2];

    args->embedFlag = !strcmp(argv[3], "e");
    args->extractFlag = !strcmp(argv[3], "x");

	if (args->embedFlag) {
        args->userPassword = argv[4];
        args->embedMessage = argv[5];
        args->message_size = strlen(args->embedMessage)*8; // size in bits
        
        printf("We are embedding \ninput pic is %s \noutput pic is %s \nembedMessageis %s \nuserPassword is %s \n\nmessageSize is %ld", args->inputname, args->outputname, args->embedMessage, args->userPassword, args->message_size);
	}
	else if (args->extractFlag) {
        args->userPassword = argv[4];
        args->message_size = 88;
        
        printf("We are extracting \ninput pic is %s \nmessageSize is %ld \nuserPassword is %s \n", args->inputname, args->message_size, args->userPassword);
	}
    else {
        printf("What is %s?\n", argv[3]);
        exit(1);
    }
}

// jpeg data
static struct jpeg_error_mgr jerr;
static jvirt_barray_ptr *coef_arrays;
static size_t block_row_size[MAX_COMPONENTS];
static JDIMENSION width_in_blocks[MAX_COMPONENTS];
static JDIMENSION height_in_blocks[MAX_COMPONENTS];
static int num_components;
static JBLOCKARRAY row_ptrs[MAX_COMPONENTS];

static void read_DCT(const char *inputname, JBLOCKARRAY *coef_buffers, j_compress_ptr outputinfo)
{
    FILE * input_file = fopen(inputname, "rb");
    if (input_file == NULL) {
        perror(inputname);
        exit(EXIT_FAILURE);
    }

    struct jpeg_decompress_struct inputinfo;

    /* Initialize the JPEG compression and decompression objects with default error handling. */
    inputinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&inputinfo);
    
    /* Specify data source for decompression and recompression */
    jpeg_stdio_src(&inputinfo, input_file);
    
    /* Read file header */
    (void) jpeg_read_header(&inputinfo, TRUE);

    num_components = inputinfo.num_components;

    j_common_ptr common = (j_common_ptr) &inputinfo;

    /* Allocate memory for reading out DCT coeffs */
    
    for (int compnum = 0; compnum < num_components; compnum++) {
        jpeg_component_info *comp = &inputinfo.comp_info[compnum];

        coef_buffers[compnum] = inputinfo.mem->alloc_barray(common,
                                                            JPOOL_IMAGE,
                                                            comp->width_in_blocks,
                                                            comp->height_in_blocks);
    }
    
    /* Read input file as DCT coeffs */
    coef_arrays = jpeg_read_coefficients(&inputinfo);
    
    /* Copy compression parameters from the input file to the output file */
    jpeg_copy_critical_parameters(&inputinfo, outputinfo);

    /* Copy DCT coeffs to a new array */
    for (int compnum = 0; compnum<num_components; compnum++)
    {
        height_in_blocks[compnum] = inputinfo.comp_info[compnum].height_in_blocks;
        width_in_blocks[compnum] = inputinfo.comp_info[compnum].width_in_blocks;
        block_row_size[compnum] = (size_t) sizeof(JCOEF)*DCTSIZE2*width_in_blocks[compnum];
        for (JDIMENSION rownum=0; rownum<height_in_blocks[compnum]; rownum++)
        {
            row_ptrs[compnum] = inputinfo.mem->access_virt_barray(common, coef_arrays[compnum], rownum, 1, FALSE);
            for (JDIMENSION blocknum=0; blocknum<width_in_blocks[compnum]; blocknum++)
            {
                for (int i=0; i<DCTSIZE2; i++)
                {
                    coef_buffers[compnum][rownum][blocknum][i] = row_ptrs[compnum][0][blocknum][i];
                }
            }
        }
    }

    jpeg_finish_decompress(&inputinfo);
    jpeg_destroy_decompress(&inputinfo);
    fclose(input_file);
}

static void init_usable_coeffs(const JBLOCKARRAY *coef_buffers, struct coefficients *usable)
{
    //First, get an array of all usable coefficients from DCT, which in our case are modes 1-4
    //buffer_coefficient usable_coefficient_array[height_in_blocks[0]*width_in_blocks[0]*4];
    usable->size = 0;
    
    //only doing component = 0 so as to only embed in luma coefficients
    for (JDIMENSION rownum=0; rownum<height_in_blocks[0]; rownum++)
    {
        for (JDIMENSION blocknum=0; blocknum<width_in_blocks[0]; blocknum++)
        {
            //printf("\n\nRow:%i, Column: %i\n", rownum, blocknum);
            for (unsigned int i=0; i<4; i++)
            {
                JCOEF this_iterations_coefficient = coef_buffers[0][rownum][blocknum][i];
                if (this_iterations_coefficient){
                    //make struct of usable coefficient with indices
                    buffer_coefficient usable_coefficient;
                    usable_coefficient.row_index = rownum;
                    usable_coefficient.column_index = blocknum;
                    usable_coefficient.block_index = i;
                    usable_coefficient.coefficient = this_iterations_coefficient;

                    usable->array[usable->size] = usable_coefficient;
                    usable->size++;
                }
            }
        }
    }
}


static node *instantiate_permutation(unsigned seed, struct coefficients *usable)
{
    srand(seed);

    int debug_i = 0;
    //instantiate a permutation of DCT coefficients, using Fisher-Yates algorithm
    size_t permutationArray[usable->size];
    
    for (size_t index = 0; index < usable->size; index++) {
        permutationArray[index]=index;
    }
    
    printf("running permutation\n");
    size_t randomIndex, temp;
    size_t maxRandom = usable->size;
    for (size_t index = 0; index < usable->size; index++) {

        randomIndex = ((unsigned int) rand()) % maxRandom--;
        temp = permutationArray[randomIndex];
        permutationArray[randomIndex] = permutationArray[index];
        permutationArray[index] = temp;
    }
    
    printf("done permuting\n");
    
    node *root = malloc(sizeof(node));
    root->next = NULL;
    node *current_node = root;
    
    printf("creating linked list...\n");

    for (size_t linked_list_index = 0; linked_list_index < usable->size; linked_list_index++) {

        //creates linked list of the coefficients, as shuffled by the permutation
        node *new_node = malloc(sizeof(node));
        new_node->coeff_struct = usable->array[permutationArray[linked_list_index]];
        new_node->debugindex = debug_i;
        debug_i++;
        add_to_linked_list(new_node, current_node);
        current_node = current_node->next;
    }
    
    printf("done creating linked list\n");
    
    current_node = root->next;
    for (int j = 0; j < 30; j++) {
        printf("%i ", current_node->coeff_struct.coefficient);
        current_node = current_node->next;
    }
    
    //print_linked_list(root);
    return root;
}

static size_t make_linked_list(const JBLOCKARRAY *coef_buffers, unsigned int seed, node **root)
{
    struct coefficients usable;
    usable.array = malloc(sizeof(buffer_coefficient)*height_in_blocks[0]*width_in_blocks[0]*4);
    if (!usable.array) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    
    init_usable_coeffs(coef_buffers, &usable);
    printf("\n\n");
    printf("buffer size is %i\n", (int) usable.size);
    *root = instantiate_permutation(seed, &usable);

    free(usable.array);
    return usable.size;
}

static FILE *setup_output(const char *outputname, j_compress_ptr outputinfo)
{
    FILE * output_file = fopen(outputname, "wb");
    if (output_file == NULL) {
        perror(outputname);
        exit(EXIT_FAILURE);
    }
    
    outputinfo->err = jpeg_std_error(&jerr);
    jpeg_create_compress(outputinfo);
    jpeg_stdio_dest(outputinfo, output_file);
    
    return output_file;
}

static void write_DCT(const char *outputname, JBLOCKARRAY *coef_buffers, j_compress_ptr outputinfo)
{
    {
        printf("writing to new picture\n");

        j_common_ptr common = (j_common_ptr) outputinfo;

        /* Output the new DCT coeffs to a JPEG file */
        for (int compnum=0; compnum<num_components; compnum++)
        {
            for (JDIMENSION rownum=0; rownum<height_in_blocks[compnum]; rownum++)
            {
                row_ptrs[compnum] = outputinfo->mem->access_virt_barray(common, coef_arrays[compnum], rownum, (JDIMENSION) 1, TRUE);
                memcpy(row_ptrs[compnum][0][0],
                       coef_buffers[compnum][rownum][0],
                       block_row_size[compnum]);
            }
        }

        /* Write to the output file */
        jpeg_write_coefficients(outputinfo, coef_arrays);

        /* Finish compression and release memory */
        jpeg_finish_compress(outputinfo);
        jpeg_destroy_compress(outputinfo);

        /* All done. */
        printf("New DCT coefficients successfully written to %s\n\n", outputname);
//        exit(jerr.num_warnings ? EXIT_FAILURE : EXIT_SUCCESS);

    }
}

static void embed(JBLOCKARRAY *coef_buffers, const char *embedMessage, node *root, size_t root_len)
{
    {
        //going along random walk as produced by the CPRNG, embed/extract message into coefficients using binary hamming matrix code
        embedMessageIntoCoefficients(embedMessage, root, root_len);
        
        //then go back and update JPEG's coefficients with the changed coefficients accordingly, using the indices data stored in the coefficient struct
        printf("changing coefficients\n");
        node *current_node = root->next;
        while (current_node != NULL) {
            JCOEF coefficient = current_node->coeff_struct.coefficient;
            coef_buffers[0][current_node->coeff_struct.row_index][current_node->coeff_struct.column_index][current_node->coeff_struct.block_index] = coefficient;
            current_node = current_node->next;
        }

        printf("done embedding\n");
    }
}

static void extract(size_t message_size, node *root, size_t usable_size)
{
    {
        node *current_node = root;
        int j = 0;
        while (j < 30) {
            printf("%i ", current_node->coeff_struct.coefficient);
            current_node = current_node->next;
            j++;
        }
    }

    printf("began extracting...\n");
    char *extractedMessage = malloc(message_size + 1);
    if (!extractedMessage) {
        perror("malloc");
        exit(1);
    }
    extractMessageFromCoefficients(root, usable_size, message_size, extractedMessage);
    printf("extractedMessage is %s\n", extractedMessage);
    free(extractedMessage);
}

int main(int argc, char * argv[])
{
    struct arguments args;
    parse_arguments(argc, argv, &args);

    struct jpeg_compress_struct outputinfo;
    JBLOCKARRAY coef_buffers[MAX_COMPONENTS];

    FILE *output = setup_output(args.outputname, &outputinfo);
    read_DCT(args.inputname, coef_buffers, &outputinfo);
    unsigned int seed = (unsigned) strtoul(args.userPassword, NULL, 10);
    node *root;
    size_t usable_size = make_linked_list(coef_buffers, seed, &root);

    if (args.embedFlag) {
        embed(coef_buffers, args.embedMessage, root, usable_size);
        write_DCT(args.outputname, coef_buffers, &outputinfo);

        extract(3488, root, usable_size);
    }
    else if (args.extractFlag) {
        extract(args.message_size, root, usable_size);
    }

    fclose(output);

    return 0;
}
