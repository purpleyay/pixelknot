//
//  f5methods.h
//  f5+dct
//
//  Created by Gwendolyn Weston on 7/20/13.
//  Copyright (c) 2013 Gwendolyn. All rights reserved.
//

#ifndef f5_dct_f5methods_h
#define f5_dct_f5methods_h

#include "node.h"

int embedMessageIntoCoefficients(const char *message, node *rootOfUsableCoefficientBuffer, size_t list_size);
void extractMessageFromCoefficients(node *rootOfUsableCoefficientBuffer, size_t list_size, size_t message_size, char *extracted_message_string);
void debug_extract(node *debug_root, node *root, size_t list_size, size_t output_buffer_size, char *output_buffer);


#endif
