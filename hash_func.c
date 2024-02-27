#include "map_tb.h"
#include "task.h"
#include "gc.h"

int hash_func(unsigned logic_addr) {
   return logic_addr % NUM_OF_HASHKEY; // return hash key
}


