//
//  node.c
//  f5+dct
//
//  Created by Gwendolyn Weston on 7/20/13.
//  Copyright (c) 2013 Gwendolyn. All rights reserved.
//

#include <stdio.h>
#include "node.h"

void add_to_linked_list(node *new_node, node *current_node) {
    new_node->prev = current_node;
    new_node->next = current_node->next;
    current_node->next = new_node;
}

void remove_from_linked_list(node *delete_node){
	delete_node->next->prev = delete_node->prev;
	delete_node->prev->next = delete_node->next;
}

node *traverse_n_nodes_forward(node *current_node, size_t n) {
	for (size_t index = 0; index<n; index++) {
        if (current_node->next == NULL) {
            printf("in trouble");
        }
        current_node = current_node->next;
	}
	return current_node;
}

node *traverse_n_nodes_backward(node *current_node, size_t n) {
	for (size_t index = 0; index < n; index++) {
        if (current_node->prev == NULL) {
            printf("in trouble");
        }
		current_node = current_node->prev;
	}
	return current_node;
}

void print_linked_list(node *root) {
    node *current_node = root->next;
    
    while(current_node != NULL) {
        printf("%i ", current_node->coeff_struct.coefficient);
        current_node = current_node->next;
    }
    
}
