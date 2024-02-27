#include "map_tb.h"
#include "task.h"
#include "gc.h"

short alloc_stream_id(char* data_type){
  short stream_id = 0;

  if(data_type[0] == 'j')                        // journal data
    stream_id = 0;
  else if(data_type[0] == 'm')                   // meta data
    stream_id = 1;
  else if(data_type[0] == 'f' || data_type[0] == 'a') // user data
    stream_id = 2;
  else                                      // etc data 
    stream_id = 3;

  return stream_id;
}
