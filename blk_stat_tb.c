#include "map_tb.h"
#include "task.h"
#include "gc.h"

void blk_st_tb_update(struct physic_addr ppn){
   blk_st_tb[ppn.ch][ppn.pln][ppn.blk].write_pt++;

   if(blk_st_tb[ppn.ch][ppn.pln][ppn.blk].write_pt >= PAGE)
      blk_st_tb[ppn.ch][ppn.pln][ppn.blk].write_pt = PAGE;

   blk_st_tb[ppn.ch][ppn.pln][ppn.blk].num_vld_pg++;

   if(blk_st_tb[ppn.ch][ppn.pln][ppn.blk].num_vld_pg >= PAGE)
      blk_st_tb[ppn.ch][ppn.pln][ppn.blk].num_vld_pg = PAGE;
}

void blk_stat_tb_initialize(){
   int i;

   for(i=0; i<CHANNEL*PLANE*BLOCK; i++){
      blk_st_tb[i/PLANE/BLOCK%CHANNEL][i/BLOCK%PLANE][i%BLOCK].last_invld_time = (DBL_MAX-1);
      blk_st_tb[i/PLANE/BLOCK%CHANNEL][i/BLOCK%PLANE][i%BLOCK].write_pt = 0;
      blk_st_tb[i/PLANE/BLOCK%CHANNEL][i/BLOCK%PLANE][i%BLOCK].num_vld_pg = 0;
      blk_st_tb[i/PLANE/BLOCK%CHANNEL][i/BLOCK%PLANE][i%BLOCK].stream_id = 5;
      blk_st_tb[i/PLANE/BLOCK%CHANNEL][i/BLOCK%PLANE][i%BLOCK].victim = 0;
      blk_st_tb[i/PLANE/BLOCK%CHANNEL][i/BLOCK%PLANE][i%BLOCK].gc_on = 0;
      pending_task[i/PLANE/BLOCK%CHANNEL][i/BLOCK%PLANE][i%BLOCK] = 0;
   }
}

void blk_st_tb_invld(struct physic_addr invld_ppn){
   int vld_pg = 0;
   unsigned i;

   for(i=0;i<PAGE;i++){
      if(blk_tb[invld_ppn.ch][invld_ppn.pln][invld_ppn.blk][i].valid == 1)
         vld_pg++;
   }
 
   blk_st_tb[invld_ppn.ch][invld_ppn.pln][invld_ppn.blk].num_vld_pg = vld_pg; 
   blk_st_tb[invld_ppn.ch][invld_ppn.pln][invld_ppn.blk].last_invld_time = nand_done_time[invld_ppn.ch].done_time;

   //fprintf(fp_blk_tb,"(I)blk_st_tb[%d][%d][%d]->write_pt ; %d, vld_pg : %d\n", invld_ppn.ch, invld_ppn.pln, invld_ppn.blk, blk_st_tb[invld_ppn.ch][invld_ppn.pln][invld_ppn.blk].write_pt, blk_st_tb[invld_ppn.ch][invld_ppn.pln][invld_ppn.blk].num_vld_pg);

 
}
