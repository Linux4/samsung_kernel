/*
 * types.h
 *
 * Created on: Aug 25, 2010
 * Author: klabadi & dlopo
 */

#ifndef TYPES_H_
#define TYPES_H_
#include <linux/types.h> 

typedef void (*handler_t)(void *);
typedef void* (*thread_t)(void *);

#endif /* TYPES_H_ */