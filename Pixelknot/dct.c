/*
* dct.c
*
* Copyright (C) 2012, Owen Campbell-Moore.
*
* This program allows you to take JPEG, modify it's DCT coefficients and
* then output the new coefficients to a new JPEG.
*
*/

#include "cdjpeg.h"   /* Common decls for compressing and decompressing jpegs */
#include "cprngssl.c"
#include <string.h>


LOCAL(void)
usage (void)
/* complain about bad command line */
{
fprintf(stderr, "\nusage: dct inputfile outputfile\n\n");
exit(EXIT_FAILURE);
}


typedef struct node node;
typedef struct buffer_coefficient buffer_coefficient;

//struct containing coefficient and metadata
struct buffer_coefficient {
  int row_index;
  int column_index;
  int block_index;
  int coefficient;
};

//linked list node
struct node{
  buffer_coefficient coefficient_struct;
  node *next;
  node *prev;
};





/*
* The main program.
*/

int
main (int argc, char **argv)
{
struct jpeg_decompress_struct inputinfo;
struct jpeg_compress_struct outputinfo;
struct jpeg_error_mgr jerr;
jvirt_barray_ptr *coef_arrays;
JDIMENSION i, compnum, rownum, blocknum;
JBLOCKARRAY coef_buffers[MAX_COMPONENTS];
JBLOCKARRAY row_ptrs[MAX_COMPONENTS];
FILE * input_file;
FILE * output_file;

/* Handle arguments */
if (argc != 3) usage();
char *inputname;
inputname = argv[1];
char *outputname;
outputname = argv[2];

/* Open the input and output files */
if ((input_file = fopen(inputname, READ_BINARY)) == NULL) {
    fprintf(stderr, "Can't open %s\n", inputname);
    exit(EXIT_FAILURE);
}
if ((output_file = fopen(outputname, WRITE_BINARY)) == NULL) {
  fprintf(stderr, "Can't open %s\n", outputname);
  exit(EXIT_FAILURE);
}

/* Initialize the JPEG compression and decompression objects with default error handling. */
inputinfo.err = jpeg_std_error(&jerr);
jpeg_create_decompress(&inputinfo);
outputinfo.err = jpeg_std_error(&jerr);
jpeg_create_compress(&outputinfo);

/* Specify data source for decompression and recompression */
jpeg_stdio_src(&inputinfo, input_file);
jpeg_stdio_dest(&outputinfo, output_file);

/* Read file header */
(void) jpeg_read_header(&inputinfo, TRUE);

/* Allocate memory for reading out DCT coeffs */
for (compnum=0; compnum<inputinfo.num_components; compnum++)
  coef_buffers[compnum] = ((&inputinfo)->mem->alloc_barray) 
                          ((j_common_ptr) &inputinfo, JPOOL_IMAGE,
                           inputinfo.comp_info[compnum].width_in_blocks,
                           inputinfo.comp_info[compnum].height_in_blocks);

/* Read input file as DCT coeffs */
coef_arrays = jpeg_read_coefficients(&inputinfo);

/* Copy compression parameters from the input file to the output file */
jpeg_copy_critical_parameters(&inputinfo, &outputinfo);

/* Copy DCT coeffs to a new array */
int num_components = inputinfo.num_components;
size_t block_row_size[num_components];
int width_in_blocks[num_components];
int height_in_blocks[num_components];
for (compnum=0; compnum<num_components; compnum++)
{
  height_in_blocks[compnum] = inputinfo.comp_info[compnum].height_in_blocks;
  width_in_blocks[compnum] = inputinfo.comp_info[compnum].width_in_blocks;
  block_row_size[compnum] = (size_t) SIZEOF(JCOEF)*DCTSIZE2*width_in_blocks[compnum];
  for (rownum=0; rownum<height_in_blocks[compnum]; rownum++)
  {
    row_ptrs[compnum] = ((&inputinfo)->mem->access_virt_barray) 
                        ((j_common_ptr) &inputinfo, coef_arrays[compnum], 
                          rownum, (JDIMENSION) 1, FALSE);
    for (blocknum=0; blocknum<width_in_blocks[compnum]; blocknum++)
    {
      for (i=0; i<DCTSIZE2; i++)
      {
        coef_buffers[compnum][rownum][blocknum][i] = row_ptrs[compnum][0][blocknum][i];
      }
    }
  }
}


//First, get a linked list of all usable coefficients from DCT, which in our case are modes 1-4

//Root node of linked list
node *root = malloc(sizeof(node)); 
node *current_node = root;

//we also need to know how large the buffer is
int usable_coefficient_buffer_size;

//Print out or modify DCT coefficients 
//only doing component = 0 so as to only embed in luma coefficients
for (rownum=0; rownum<height_in_blocks[compnum]; rownum++)
{
  for (blocknum=0; blocknum<width_in_blocks[compnum]; blocknum++)
  {
    printf("\n\nRow:%i, Column: %i\n", rownum, blocknum);
    for (i=0; i<4; i++)
    {
      int this_iterations_coefficient = coef_buffers[0][rownum][blocknum][i];
      if (this_iterations_coefficient){

        //make struct of usable coefficient with the metadata
        buffer_coefficient usable_coefficient;
        usable_coefficient.row_index = rownum;
        usable_coefficient.column_index = blocknum;
        usable_coefficient.block_index = i;
        usable_coefficient.coefficient = this_iterations_coefficient;

        printf("%i ,", this_iterations_coefficient);

        //append to end of linked list
        node *new_node = malloc(sizeof(node));
        new_node->coefficient_struct = usable_coefficient;
        new_node->prev = current_node;
        new_node->next = current_node->next;
        current_node->next = new_node;
        current_node = current_node->next;

        usable_coefficient_buffer_size++;

      }
    }
  }
}

printf("\n\n");

//print whole list for testing
current_node = root->next;
while(current_node != NULL){
  printf("%i ", current_node->coefficient_struct.coefficient);
}

//STEPS FROM HERE: 
//test linked list is working + reading in coefficients
//integrate with existing f5 skeleton in other file
//as in, change all code that was dependent on the buffer being an array so that is compatible with linked list
//add way to read in password and message text (or file...?)
//add way to choose embedding or extraction

//Still not completely solved: how to hide message length...?


/* Output the new DCT coeffs to a JPEG file */
for (compnum=0; compnum<num_components; compnum++)
{
  for (rownum=0; rownum<height_in_blocks[compnum]; rownum++)
  {
    row_ptrs[compnum] = ((&outputinfo)->mem->access_virt_barray) 
                        ((j_common_ptr) &outputinfo, coef_arrays[compnum], 
                         rownum, (JDIMENSION) 1, TRUE);
    memcpy(row_ptrs[compnum][0][0], 
           coef_buffers[compnum][rownum][0],
           block_row_size[compnum]);
  }
}

/* Write to the output file */
jpeg_write_coefficients(&outputinfo, coef_arrays);

/* Finish compression and release memory */
jpeg_finish_compress(&outputinfo);
jpeg_destroy_compress(&outputinfo);
jpeg_finish_decompress(&inputinfo);
jpeg_destroy_decompress(&inputinfo);

/* Close files */
fclose(input_file);
fclose(output_file);

/* All done. */
printf("New DCT coefficients successfully written to %s\n\n", outputname);
exit(jerr.num_warnings ? EXIT_WARNING : EXIT_SUCCESS);
return 0;     /* suppress no-return-value warnings */
}
