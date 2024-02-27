#include "map_tb.h"
#include "task.h"
#include "gc.h"

void blk_tb_update(struct physic_addr ppn){
   if( ppn.valid == 1){
      if( (sramq+sramq_pt)->first != 1 ){
         blk_tb[prev_ppn.ch][prev_ppn.pln][prev_ppn.blk][prev_ppn.pg].next_page = ppn;
         blk_tb[ppn.ch][ppn.pln][ppn.blk][ppn.pg].prev_page = prev_ppn;
      }

      prev_ppn = ppn;
      blk_tb[ppn.ch][ppn.pln][ppn.blk][ppn.pg].valid = 1;
      blk_tb[ppn.ch][ppn.pln][ppn.blk][ppn.pg].lpn   = (sramq+sramq_pt)->lpn;
      blk_tb[ppn.ch][ppn.pln][ppn.blk][ppn.pg].first = (sramq+sramq_pt)->first;
      blk_tb[ppn.ch][ppn.pln][ppn.blk][ppn.pg].last  = (sramq+sramq_pt)->last;        

      blk_st_tb_update(ppn);     
   }
   else
      return;
}
