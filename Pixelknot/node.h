//
//  node.h
//  f5+dct
//
//  Created by Gwendolyn Weston on 7/20/13.
//  Copyright (c) 2013 Gwendolyn. All rights reserved.
//

#ifndef f5_dct_node_h
#define f5_dct_node_h

//struct containing coefficient and metadata
struct buffer_coefficient {
    unsigned int row_index;
    unsigned int column_index;
    unsigned int block_index;
    short coefficient;
};

typedef struct buffer_coefficient buffer_coefficient;

typedef struct node node;

//linked list node
struct node{
    buffer_coefficient coeff_struct;
    int debugindex;
    node *next;
    node *prev;
};

void add_to_linked_list(node *new_node, node *current_node);
void remove_from_linked_list(node *delete_node);
node *traverse_n_nodes_forward(node *current_node, size_t n);
node *traverse_n_nodes_backward(node *current_node, size_t n);
void print_linked_list(node *root);



#endif
