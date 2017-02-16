#include <stdio.h>
#include <stdlib.h>
#include <openssl/rand.h>
#include <limits.h>

unsigned char *spc_rand(unsigned char *buf, size_t l) {
       if (!RAND_bytes(buf, l)) {
         fprintf(stderr, "The PRNG is not seeded!\n");
		 abort( ); 
		}
		
		return buf; 
	}

unsigned int spc_rand_uint(void) {
      unsigned int res;
      spc_rand((unsigned char *)&res, sizeof(unsigned int));
	  return res; 
}

int spc_rand_range(int min, int max){
	unsigned int rado;
	int range = max - min + 1;
	
	if (max<min) abort();
	
	do{
		rado = spc_rand_uint();
	} while (rado >UINT_MAX - (UINT_MAX % range));
	
	return min + (rado %range);
}