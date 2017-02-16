
//
//  f5methods.c
//  f5+dct
//
//  Created by Gwendolyn Weston on 7/20/13.
//  Copyright (c) 2013 Gwendolyn. All rights reserved.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "f5algorithm.h"
#include "node.h"

typedef uint64_t kbits;

#define BINARY "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
(byte & 0x80 ? '1' : '0'), \
(byte & 0x40 ? '1' : '0'), \
(byte & 0x20 ? '1' : '0'), \
(byte & 0x10 ? '1' : '0'), \
(byte & 0x08 ? '1' : '0'), \
(byte & 0x04 ? '1' : '0'), \
(byte & 0x02 ? '1' : '0'), \
(byte & 0x01 ? '1' : '0')

static void print_string_binary(const char *string) {
    for (int i = 0; i < strlen(string); i++) {
        printf(""BINARY"", BYTE_TO_BINARY(string[i]));
    }
    printf("\n");
}

static size_t get_message_partition_size(size_t message_size_in_bits, size_t list_size) {
    size_t message_partition_size = 1;
    float embed_rate = (float)message_size_in_bits/list_size;

    while (embed_rate < ((float)message_partition_size/((1<<message_partition_size)-1))){
        message_partition_size++;
    }
    message_partition_size -= 2; // just to be safe in case there's lots of zeroes

    return message_partition_size;
}

int just_once = 0;
static node *fill_coeff_buffer_with_n_coefficients_lsb (uint8_t *lsb_buffer, node *coeff_node, size_t n) {
    for (size_t i = 0; i < n; i++){
        while (coeff_node->coeff_struct.coefficient==0) {
            remove_from_linked_list(coeff_node);
            coeff_node = coeff_node->next;
        }
//        if (!just_once) printf("%i ", coeff_node->coeff_struct.coefficient);

        lsb_buffer[i] = (coeff_node->coeff_struct.coefficient & 1);
        
        if (coeff_node->next == NULL) {
            printf("sorry, you ran out of coefficients");
            assert(coeff_node->next != NULL);
        }
        coeff_node = coeff_node->next;
    }
//    just_once = 1;
    return coeff_node;
}

static node *fill_buffer (uint8_t *lsb_buffer, node *coeff_node, size_t n, node *array) {
    for (size_t i = 0; i < n; i++){
        while (coeff_node->coeff_struct.coefficient==0) {
            remove_from_linked_list(coeff_node);
            coeff_node = coeff_node->next;
        }

        lsb_buffer[i] = (coeff_node->coeff_struct.coefficient & 1);

        if (coeff_node->next == NULL) {
            printf("sorry, you ran out of coefficients");
            assert(coeff_node->next != NULL);
        }
        coeff_node = coeff_node->next;
    }
    return coeff_node;
}

//guys guys guys, the math behind this part is so fucking cool
//i wish the implementation didn't obscure it so much
//but basically, multiply each entry in buffer by its index (starting from 1) and XOR all these products
static kbits hash_coefficient_buffer(const uint8_t *lsb_buffer, size_t buffer_len) {
    kbits hash = 0;
    for (kbits i = 0, n = buffer_len; i < n; i++){
        kbits product = ((kbits) lsb_buffer[i]) * (i+1);
        hash ^= product;
    }
    return hash;
}

static const char *fill_message_buffer_with_k_message_bits (kbits *const message_buffer_ptr, const char *message, size_t k, int *current_msgbit_index) {
    *message_buffer_ptr = 0;

    for (size_t i = k; i > 0; i--){
        if (*current_msgbit_index > 7){
            message++;
            *current_msgbit_index = 0;
        }

        // we may have hit the end of the string
        if (!*message)
            break;

        //get next bit of message, starting from where we last left off (relative to the current byte we're on)
        int message_bit = (*message & (1 << (7 - *current_msgbit_index))) ? 1 : 0;
        *current_msgbit_index += 1;

        //store bit in this iteration's message buffer
        if (message_bit) *message_buffer_ptr |= (1 << (i-1));
    }

    return message;
}

static int embed_message_bits(size_t index_to_change, size_t codeword_size, node *current_ucb_node) {

    if (!index_to_change) {
        return 0; //if zero, there's nothing to embed
    }

    node *node_to_change = traverse_n_nodes_backward(current_ucb_node, codeword_size);
    node_to_change = traverse_n_nodes_forward(node_to_change, index_to_change);

    buffer_coefficient *coeff = &node_to_change->coeff_struct;
    if (coeff->coefficient > 0)
        coeff->coefficient--;
    else
        coeff->coefficient++;

    if (coeff->coefficient == 0){
        current_ucb_node = traverse_n_nodes_backward(current_ucb_node, codeword_size);
        return 1;
    }
    else {
        return 0;
    }
}

int embedMessageIntoCoefficients(const char *message, node *rootOfUsableCoefficientBuffer, size_t list_size){
    size_t message_partition_size = get_message_partition_size(strlen(message)*8, list_size); //k
    assert(message_partition_size <= 64);

    size_t codeword_size = (1<<message_partition_size)-1; //n

    print_string_binary(message);

    if (message_partition_size < 2){
        return -1;
    }
    else {

        kbits message_buffer = 0;
        int shrinkage_flag = 0;
        int current_msgbit_index = 0;

        node *current_ucb_node = rootOfUsableCoefficientBuffer->next;
        node *debug_node = rootOfUsableCoefficientBuffer->next;
        node *second_debug_node = rootOfUsableCoefficientBuffer->next;

        while (*message != '\0' && current_ucb_node) {

            uint8_t lsb_buffer[codeword_size];

            memset(lsb_buffer, 0, codeword_size*sizeof(uint8_t));
            current_ucb_node = fill_coeff_buffer_with_n_coefficients_lsb(lsb_buffer, current_ucb_node, codeword_size);
            kbits hash = hash_coefficient_buffer(lsb_buffer, codeword_size);

            //if shrinkage is true, then the correct bits will already be the message_buffer variable
            if (!shrinkage_flag) {
                message = fill_message_buffer_with_k_message_bits(&message_buffer, message, message_partition_size, &current_msgbit_index);
            }
            printf("%llu ", message_buffer);

            if (!*message)
                break;

            size_t index_to_change = (message_buffer ^ hash - 1);
            assert(index_to_change >= 0);


            shrinkage_flag = embed_message_bits(index_to_change, codeword_size, current_ucb_node);


        } //end while (*message)
        
        if (*message != '\0' && current_ucb_node == NULL){
            printf("NOT ENOUGH CAPACITY\n");
            return 0;
        }
    } //end if (doEmbed)

    printf("message = %s\n", message);

    return 1;
        
}

void extractMessageFromCoefficients(node *rootOfUsableCoefficientBuffer, size_t list_size, size_t output_buffer_size, char *output_buffer) {
    size_t message_partition_size = get_message_partition_size(output_buffer_size, list_size); // k
    size_t message_size_in_bytes = output_buffer_size/8;

    size_t message_index = 0; //how many bytes along have been extracted
    int message_bit_index = 0; //how many bits along (relative to current byte) have been extracted

    //calculate the code word length n = 2^k - 1
    size_t codeword_size = (1<<message_partition_size)-1;  //let this also be known as the variable n
    node *current_ucb_node = rootOfUsableCoefficientBuffer->next; //mark how far along ucb list we are.
    uint8_t coeff_buffer[codeword_size]; //buffer to store n bits of codeword
    
    while (message_index < message_size_in_bytes){
        
        current_ucb_node = fill_coeff_buffer_with_n_coefficients_lsb(coeff_buffer, current_ucb_node, codeword_size);
        kbits hash = hash_coefficient_buffer(coeff_buffer, codeword_size);
        printf("%llu ", hash);

        //add each bit from k-size hash, starting from the left into current index of extracted message array
        for (size_t current_hash_bit = 0; current_hash_bit < message_partition_size; current_hash_bit++){
            if (message_bit_index > 7){ //we reach end of byte, so increment to next byte of allocated array
                message_bit_index = 0;
                message_index++;
                output_buffer[message_index] = 0;
            }
            
            //get left-most bit from hash
            kbits bitmask = 1 << ((kbits) message_partition_size - current_hash_bit - 1);
            int message_bit = (hash & bitmask) ? 1 : 0;

            //add bit to message's current byte
            if (message_bit) output_buffer[message_index] |= (1 << (7-message_bit_index));
            message_bit_index++;
            
        }

    } //end while (message_index < message_length)

}// end extraction
