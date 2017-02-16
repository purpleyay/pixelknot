//
//  MinUnit.h
//  Pixelknot
//
//  Created by gwendolyn weston on 2/15/17.
//  Copyright © 2017 gwendolyn weston. All rights reserved.
//

#ifndef MinUnit_h
#define MinUnit_h

#define mu_assert(message, test) do { if (!(test)) return message; } while (0)
#define mu_run_test(test) do { char *message = test(); tests_run++; \
if (message) return message; } while (0)
extern int tests_run;

#endif 
