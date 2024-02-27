#include "map_tb.h"
#include "task.h"
#include "gc.h"

struct physic_addr alloc_ppn(short ch_num, short stream_id) {
  struct physic_addr new_ppn;
  unsigned i = 0;

  if(ch_num < 0){
    new_ppn.ch = 0;
    new_ppn.pln = 0;
    new_ppn.blk = 0;
    new_ppn.pg = 0;
    new_ppn.valid = 0;

    return new_ppn;
  }

  if( (sramq+sramq_pt)->valid == 1 && (sramq+sramq_pt)->task_time <= global_time ){
    if(fmc_port_busy[ch_num] == 0 && fmc_buff[ch_num] < 2) {
      int8_t research = 0;

      if(blk_st_tb[ch_num][access_ppn[ch_num][stream_id].plne][access_ppn[ch_num][stream_id].blck].stream_id != stream_id){
	research = 1;
      }     

      if(blk_st_tb[ch_num][access_ppn[ch_num][stream_id].plne][access_ppn[ch_num][stream_id].blck].write_pt >= PAGE){
	blk_st_tb[ch_num][access_ppn[ch_num][stream_id].plne][access_ppn[ch_num][stream_id].blck].write_pt = PAGE;
	research = 1;
      }

      if(research){ 
	while(blk_st_tb[ch_num][i/BLOCK%PLANE][i%BLOCK].write_pt != 0){ // Search free block  
	  i++;   

	  if(i == BLOCK*PLANE){ // The channel is full have to do GC
	    while(vll->head != NULL && vll->head->victim_time <= global_time){
	      garbage_collection(vll->head, vll->head->chnl, 1);
	    }
	    
	    break;           
	  }
	}

	if(i == BLOCK*PLANE){
	  i = 0;

	  while(blk_st_tb[ch_num][i/BLOCK%PLANE][i%BLOCK].write_pt == PAGE) // Search free block  
	    i++;   
	}

	access_ppn[ch_num][stream_id].plne = i/BLOCK%PLANE;
	access_ppn[ch_num][stream_id].blck = i%BLOCK;
	total_free_blk--;
      }                  

      if(blk_st_tb[ch_num][access_ppn[ch_num][stream_id].plne][access_ppn[ch_num][stream_id].blck].write_pt == 0)
	total_free_blk--; 
   
      new_ppn.ch    = ch_num;
      new_ppn.pln   = access_ppn[ch_num][stream_id].plne;
      new_ppn.blk   = access_ppn[ch_num][stream_id].blck;
      new_ppn.pg    = blk_st_tb[ch_num][access_ppn[ch_num][stream_id].plne][access_ppn[ch_num][stream_id].blck].write_pt;
      new_ppn.valid = 1;
      blk_st_tb[ch_num][access_ppn[ch_num][stream_id].plne][access_ppn[ch_num][stream_id].blck].stream_id = stream_id;
   
      lpn_ppn_map_tb[(sramq+sramq_pt)->lpn] = new_ppn;
      lpn_ppn_map_tb[(sramq+sramq_pt)->lpn].valid = 1;         
   
      return new_ppn;
    }
    else{
      new_ppn.ch = 0;
      new_ppn.pln = 0;
      new_ppn.blk = 0;
      new_ppn.pg = 0;
      new_ppn.valid = 0;

      return new_ppn;
    }
  }
  else{
    new_ppn.ch = 0;
    new_ppn.pln = 0;
    new_ppn.blk = 0;
    new_ppn.pg = 0;
    new_ppn.valid = 0;

    return new_ppn;
  }
}

void phys_addr_invalid(unsigned lpn){
  struct physic_addr invld_ppn = lpn_ppn_map_tb[lpn];   

  // lpn2ppn table invalid
  lpn_ppn_map_tb[lpn].valid = 2;	
 
  // block & block status table update
  blk_tb[invld_ppn.ch][invld_ppn.pln][invld_ppn.blk][invld_ppn.pg].valid = 2;   
  blk_st_tb_invld(invld_ppn);
  vll_update(invld_ppn);
  total_invld_pg++;
   
  // return lpn
  lpn_pool[lpn] = 0;   
}


