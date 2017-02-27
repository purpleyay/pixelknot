
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

static void print_string_binary(const char *string, int k) {
    for (int i = 0; i < strlen(string); i++) {
        printf(""BINARY"", BYTE_TO_BINARY(string[i]));
    }
}

void print_debug_linked_list(node *root, size_t n) {
    node *current_node = root->next;

    for (size_t i = 0; i < n; i++) {
        printf("%i ", current_node->coeff_struct.coefficient);
        current_node = current_node->next;
    }
    printf ("\n");
}


static size_t get_message_partition_size(size_t message_size_in_bits, size_t list_size) {
    size_t k = 1;
    float embed_rate = (float)message_size_in_bits/list_size;

    while (embed_rate < ((float)k/((1<<k)-1))){
        k++;
    }
    k -= 2; // just to be safe in case there's lots of zeroes

    return k;
}

int just_once = 0;
static node *fill_coeff_buffer_with_n_coefficients_lsb (uint8_t *lsb_buffer, node *coeff_node, size_t n) {
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

static int equalArrays(short *a1, short *a2, size_t size) {
    for (int i = 0; i < size; i++) {
        if (a1[i] != a2[i]) {
            printf("numbers untrue: %i != %i, index: %i\n", a1[i], a2[i], i);
            for (int j = 0; j < 10; j++) {
                printf("next numbers: %i != %i \n", a1[j], a2[j]);
            }
            return 0;
        }
    }
    return 1;
}

static int unequalArray(short *a1, short *a2, size_t size, size_t unequal) {
    for (int i = 0; i < size; i++) {
        if (a1[i] != a2[i]) {
            if (i != unequal) {
                printf("numbers untrue: %i != %i, %i index is incorrect\n", a1[i], a2[i], i);
                return 0;
            }
        }
    }
    return 1;
}

static node *fill_buffer_counting (uint8_t *lsb_buffer, node *coeff_node, size_t n, short *array, int *zero_index, node **node_array) {
    for (size_t i = 0; i < n; i++){
        while (coeff_node->coeff_struct.coefficient==0) {
            *zero_index+=1;
            coeff_node = coeff_node->next;
        }

        lsb_buffer[i] = (coeff_node->coeff_struct.coefficient & 1);
        array[i] = coeff_node->coeff_struct.coefficient;

        if (coeff_node->next == NULL) {
            printf("sorry, you ran out of coefficients");
            assert(coeff_node->next != NULL);
        }
        node_array[i] = coeff_node;
        coeff_node = coeff_node->next;
    }
    return coeff_node;
}

static node *fill_buffer (uint8_t *lsb_buffer, node *coeff_node, size_t n, short *array) {
    for (size_t i = 0; i < n; i++){
        while (coeff_node->coeff_struct.coefficient==0) {
            remove_from_linked_list(coeff_node);
            coeff_node = coeff_node->next;
        }

        lsb_buffer[i] = (coeff_node->coeff_struct.coefficient & 1);
        array[i] = coeff_node->coeff_struct.coefficient;

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
//        printf("%i", message_bit);
    }

//    printf(" is ");
    return message;
}

static int embed_message_bits(size_t index_to_change, size_t n, node *current_node) {

    if (!index_to_change) {
        return 0; //if zero, there's nothing to embed
    }

    node *node_to_change = traverse_n_nodes_backward(current_node, n);
    node_to_change = traverse_n_nodes_forward(node_to_change, index_to_change);

    buffer_coefficient *coeff = &node_to_change->coeff_struct;
    if (coeff->coefficient > 0)
        coeff->coefficient--;
    else
        coeff->coefficient++;

    if (coeff->coefficient == 0){
        current_node = traverse_n_nodes_backward(current_node, n);
        return 1;
    }
    else {
        return 0;
    }
}

static int embed_message (size_t index_to_change, size_t n, node **node_array, node *debug_node) {
//    node *node_to_change = traverse_n_nodes_backward(current_node, n);
//    if (node_to_change->debugindex != debug_node->debugindex) {
////        printf("not starting where head node is");
//    }

//    node_to_change = traverse_n_nodes_forward(node_to_change, index_to_change);
    node *node_to_change = node_array[index_to_change];
    buffer_coefficient *coeff = &node_to_change->coeff_struct;
    if (coeff->coefficient > 0)
        coeff->coefficient--;
    else
        coeff->coefficient++;

    if (coeff->coefficient == 0)
        return 1;
    else
        return 0;
}

void printArray(short *array, size_t n) {
    for (int i = 0; i<n; i++) {
        printf("%i ", array[i]);
    }
}

int embedMessageIntoCoefficients(const char *message, node *root, size_t list_size){
    size_t k = get_message_partition_size(strlen(message)*8, list_size); //k
    assert(k <= 64);

    size_t n = (1<<k)-1; //n

    if (k < 2){
        return -1;
    }
    else {
        kbits message_buffer = 0;
        int shrinkage_flag = 0;
        int current_msgbit_index = 0;

        node *current_node = root->next;
        node **node_array = malloc(n * sizeof(node*));
//        print_string_binary(message, k);

        printf("embed hash\n");

        while (*message != '\0' && current_node) {
            int zero_index = 0;

            uint8_t lsb_buffer[n];
            memset(lsb_buffer, 0, n*sizeof(uint8_t));

            /// DEBUG IN - keep a node where the current node started
            short coeff_array[n];
            memset(coeff_array, 0, n*sizeof(short));

            node *debug_node = current_node;
            int debugindex = debug_node->debugindex;

            /// DEBUG OUT

            current_node = fill_buffer_counting(lsb_buffer, current_node, n, coeff_array, &zero_index, node_array);
//            current_node = fill_buffer(lsb_buffer, current_node, n, coeff_array);

            kbits hash = hash_coefficient_buffer(lsb_buffer, n);

//            printf("\n");
//            for (int i = 0; i<n; i++) {
//                printf("%i ", coeff_array[i]);
//            }
//            printf("\n");

            //if shrinkage is true, then the correct bits will already be the message_buffer variable
            if (!shrinkage_flag) {
                message = fill_message_buffer_with_k_message_bits(&message_buffer, message, k, &current_msgbit_index);
            }

            if (!*message)
                break;

            if ((message_buffer^hash) == 0) { // change nothing, message already embedded
                shrinkage_flag = 0;
                continue;
            }

            size_t index_to_change = ((message_buffer ^ hash) - 1);
            assert(index_to_change >= 0);
            assert(index_to_change <= n);


            shrinkage_flag = embed_message(index_to_change, n, node_array, debug_node);
            if (debugindex == 395) {
                for (int i = 0; i<n; i++) {
                    printf("%i ", coeff_array[i]);
                }
                printf("/n");
            }
            if (shrinkage_flag) {
                current_node = traverse_n_nodes_backward(current_node, n+zero_index);
                if (current_node->debugindex != debug_node->debugindex) {
//                    printf("not traversing back to head node");
                }
            }

            if (!shrinkage_flag) {
                printf("%llu ", message_buffer);
            }

            /// DEBUG IN
            if (!shrinkage_flag) {
                uint8_t coeff_buffer[n]; //buffer to store n bits of codeword
                short debug_array[n];
                memset(debug_array, 0, n*sizeof(short));

                fill_buffer(coeff_buffer, debug_node, n, debug_array);
                if (!unequalArray(coeff_array, debug_array, n, index_to_change)) {
                    printf("arrays are not equal");
                }

                kbits debughash = hash_coefficient_buffer(coeff_buffer, n);
                if (message_buffer != debughash) {
                    printf("not embedding what message is");
                }
            }
            /// DEBUG OUT

        } //end while (*message)
        
        if (*message != '\0' && current_node == NULL){
            printf("NOT ENOUGH CAPACITY\n");
            return 0;
        }
    } //end if (doEmbed)

    printf("message = %s\n", message);

    return 1;
        
}

void extractMessageFromCoefficients(node *root, size_t list_size, size_t output_buffer_size, char *output_buffer) {
    size_t k = get_message_partition_size(output_buffer_size, list_size); // k
    size_t message_size_in_bytes = output_buffer_size/8;

    size_t message_index = 0; //how many bytes along have been extracted
    int message_bit_index = 0; //how many bits along (relative to current byte) have been extracted

    //calculate the code word length n = 2^k - 1
    size_t n = (1<<k)-1;  //let this also be known as the variable n
    node *current_node = root->next; //mark how far along ucb list we are.
    uint8_t coeff_buffer[n]; //buffer to store n bits of codeword

    printf("extract hash\n");
    while (message_index < message_size_in_bytes){

//        current_node = fill_coeff_buffer_with_n_coefficients_lsb(coeff_buffer, current_node, n);

        /// DEBUG IN
        short coeff_array[n];
        memset(coeff_array, 0, n*sizeof(short));
        node *debug_node = current_node;
        int debugindex = debug_node->debugindex;

        current_node = fill_buffer(coeff_buffer, current_node, n, coeff_array);
        /// DEBUG OUT

        kbits hash = hash_coefficient_buffer(coeff_buffer, n);

        printf("%llu ", hash);

        //add each bit from k-size hash, starting from the left into current index of extracted message array
        for (size_t current_hash_bit = 0; current_hash_bit < k; current_hash_bit++){
            if (message_bit_index > 7){ //we reach end of byte, so increment to next byte of allocated array
                message_bit_index = 0;
                message_index++;
                output_buffer[message_index] = 0;
            }
            
            //get left-most bit from hash
            kbits bitmask = 1 << ((kbits) k - current_hash_bit - 1);
            int message_bit = (hash & bitmask) ? 1 : 0;

            //add bit to message's current byte
            if (message_bit) output_buffer[message_index] |= (1 << (7-message_bit_index));
            message_bit_index++;
            
        }

    } //end while (message_index < message_length)

}// end extraction

void debug_extract(node *debug_root, node *picture_root, size_t list_size, size_t output_buffer_size, char *output_buffer) {
    size_t k = get_message_partition_size(output_buffer_size, list_size); // k
    size_t message_size_in_bytes = output_buffer_size/8;

    size_t message_index = 0; //how many bytes along have been extracted
    int message_bit_index = 0; //how many bits along (relative to current byte) have been extracted

    //calculate the code word length n = 2^k - 1
    size_t n = (1<<k)-1;  //let this also be known as the variable n
    node *current_node = picture_root->next; //mark how far along ucb list we are.
    node *debug_node = debug_root->next; //mark how far along ucb list we are.

    uint8_t coeff_buffer[n]; //buffer to store n bits of codeword
    uint8_t debug_buffer[n]; //buffer to store n bits of codeword

    printf("extract hash\n");
    while (message_index < message_size_in_bytes){
        int referenceindex = current_node->debugindex; //for printing
        node *referencenode = current_node; //for printing

//        if (current_node->coeff_struct.coefficient != debug_node->coeff_struct.coefficient) {
//            printf("debug stop");
//        }

        /// DEBUG IN
        short coeff_array[n];
        memset(coeff_array, 0, n*sizeof(short));
        current_node = fill_buffer(coeff_buffer, current_node, n, coeff_array);
        kbits hash = hash_coefficient_buffer(coeff_buffer, n);
        printf("%llu ", hash);

        short debug_array[n];
        memset(debug_array, 0, n*sizeof(short));
        debug_node = fill_buffer(debug_buffer, debug_node, n, debug_array);
        kbits debughash = hash_coefficient_buffer(debug_buffer, n);

//        if (!equalArrays(coeff_array, debug_array, n)) {
//            printf("stop here");
//        }
//        if (hash != debughash) {
//            printf("stop here");
//        }
        /// DEBUG OUT



        //add each bit from k-size hash, starting from the left into current index of extracted message array
        for (size_t current_hash_bit = 0; current_hash_bit < k; current_hash_bit++){
            if (message_bit_index > 7){ //we reach end of byte, so increment to next byte of allocated array
                message_bit_index = 0;
                message_index++;
                output_buffer[message_index] = 0;
            }

            //get left-most bit from hash
            kbits bitmask = 1 << ((kbits) k - current_hash_bit - 1);
            int message_bit = (hash & bitmask) ? 1 : 0;

            //add bit to message's current byte
            if (message_bit) output_buffer[message_index] |= (1 << (7-message_bit_index));
            message_bit_index++;

        }

    } //end while (message_index < message_length)
    
}// end extraction

