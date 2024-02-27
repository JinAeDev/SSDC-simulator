#include "map_tb.h"
#include "task.h"
#include "gc.h"

void check_gc_possible(){
  int8_t pass = 0;

  if(read_queue == NULL)
    pass = 0;
  else
    pass = 1;

  if(vll->head != NULL){
    if(pass == 1){
      if(vll->head->victim_time <= global_time && read_queue->arrive_time >= global_time){
	blk_st_tb[vll->head->chnl][vll->head->plne][vll->head->blck].gc_on = 1;
      }
    }
    else{
      if(vll->head->victim_time <= global_time){
	blk_st_tb[vll->head->chnl][vll->head->plne][vll->head->blck].gc_on = 1;
      }
    }
  }  
}

void vll_update(struct physic_addr invld_ppn){
  struct victim_block* victim_blk = search_victim_blk(invld_ppn);
  int invld_pg = 0;
   
  if(victim_blk == NULL){
    if(blk_st_tb[invld_ppn.ch][invld_ppn.pln][invld_ppn.blk].write_pt == PAGE){
      invld_pg = blk_st_tb[invld_ppn.ch][invld_ppn.pln][invld_ppn.blk].write_pt - blk_st_tb[invld_ppn.ch][invld_ppn.pln][invld_ppn.blk].num_vld_pg;

      if(invld_pg >= GC_THRESHOLD_BACK*PAGE) { // The percentage of "Invalid pages" is more than 60%.
	blk_st_tb[invld_ppn.ch][invld_ppn.pln][invld_ppn.blk].last_invld_time = nand_done_time[invld_ppn.ch].done_time;
	blk_st_tb[invld_ppn.ch][invld_ppn.pln][invld_ppn.blk].victim = 1;
	insert_vll(invld_ppn);         
      }
    }
  }
}

struct victim_block* search_victim_blk(struct physic_addr invld_ppn){
  struct victim_block* search_node = vll->head;

  while(search_node != NULL){
    if(search_node->chnl == invld_ppn.ch){
      if(search_node->plne == invld_ppn.pln){
	if(search_node->blck == invld_ppn.blk){
	  break;            
	}
      }
    }
    search_node = search_node->next;
  }

  return search_node;
}

void insert_vll(struct physic_addr invld_ppn){
  struct victim_block* new_node = (struct victim_block*)malloc(sizeof(struct victim_block));

  new_node->chnl = invld_ppn.ch;
  new_node->plne = invld_ppn.pln;
  new_node->blck = invld_ppn.blk;
  new_node->victim_time = nand_done_time[invld_ppn.ch].done_time;
  new_node->cp_done = 0;
  new_node->erase_done = 0;
  new_node->next = NULL;
   
  if(vll->head == NULL && vll->tail == NULL){
    vll->head = vll->tail = new_node;
  }
  else{
    vll->tail->next = new_node;
    vll->tail = vll->tail->next;
  }
  num_of_vic_blk++;
}

/*void background_garbage_collection(){ 
  short gc_done = 0;
 
  if(vll->head != NULL){ // "victim block" exists.
    if(nand_busy[vll->head->chnl] == 0){
      while(vll->head->victim_time <= global_time){
	gc_on[vll->head->chnl] = 1;
	//	gc_done = garbage_collection(vll->head, vll->head->chnl, 0);  
	
	if(gc_done)
	  delete_vll(vll->head);
	else
	  break; 
	
	if(vll->head == NULL)
	  break;
      }
    }
  }
}

void foreground_garbage_collection(){
  short gc_done = 0;
  short gc_ch;

  for(gc_ch=0;gc_ch<CHANNEL;gc_ch++){
    if(total_invld_pg >= PAGE){
      if(total_free_blk <= (PLANE*BLOCK) + buff_task/PAGE){
	if(vll->head == NULL)
	  insert_min_invld_blk(gc_ch); // -ing
            
	while(vll->head != NULL){
	  gc_on[vll->head->chnl] = 1;
	  //gc_done = garbage_collection(vll->head, vll->head->chnl, 1);  
   
	  if(gc_done)
	    delete_vll(vll->head);
	  else
	    break;
	}
      }
    }
  }
  }*/

/*void insert_min_invld_blk(short ch){
  int free_pg_num = 0;
  int max_invld_pg_num = 0;
  int pln_num = 0;
  int blk_num = 0;
  int wp = 0;
  int vld_pg = 0;
  int i;

  for(i=0;i<PLANE*BLOCK;i++){ // Check all blocks in channel
    if(blk_st_tb[ch][i/BLOCK%PLANE][i%BLOCK].victim == 0){
      if(blk_st_tb[ch][i/BLOCK%PLANE][i%BLOCK].write_pt == PAGE){
	if(blk_st_tb[ch][i/BLOCK%PLANE][i%BLOCK].last_invld_time <= global_time){
	  wp = blk_st_tb[ch][i/BLOCK%PLANE][i%BLOCK].write_pt;
	  vld_pg = blk_st_tb[ch][i/BLOCK%PLANE][i%BLOCK].num_vld_pg;

	  if((wp-vld_pg) >= GC_THRESHOLD_FORE * PAGE (total_free_blk-buff_task/PAGE)/(PLANE*BLOCK+AGED_STORAGE)){
	    //if(max_invld_pg_num > (wp - vld_pg)){
	    pln_num = i/BLOCK%PLANE;
	    blk_num = i%BLOCK;
	    
	    vll_jump_insert(ch, pln_num, blk_num);
	    max_invld_pg_num = wp - vld_pg;
	    free_pg_num += max_invld_pg_num;
	    //}
	  }
	}
      }
    }
    if(free_pg_num > buff_task)
      break;
  }
}

void vll_jump_insert(int ch, int pln, int blk){
  struct victim_block* new_node = (struct victim_block*)malloc(sizeof(struct victim_block));
   
  new_node->chnl = ch;
  new_node->plne = pln;
  new_node->blck = blk;
  new_node->victim_time = nand_done_time[ch].done_time;
  new_node->next = NULL;

  blk_st_tb[ch][pln][blk].victim = 1;

  if(vll->head == NULL){
    vll->head = new_node;
  }
  else{
    new_node->next = vll->head;
    vll->head = new_node;
  }
  }*/

void delete_vll(struct victim_block* remove_node, short mode){
  // Store info of recent erased block
  if((remove_node->cp_done == 1 && remove_node->erase_done == 1) || mode){
    struct victim_block* new_node = (struct victim_block*)malloc(sizeof(struct victim_block));
  
    new_node->chnl = remove_node->chnl;
    new_node->plne = remove_node->plne;
    new_node->blck = remove_node->blck;
    new_node->cp_done = remove_node->cp_done;
    new_node->erase_done = remove_node->erase_done;
    new_node->next = NULL;  

    // Insert Recent GC queue to update global time
    if(recent_gc_queue == NULL){
      recent_gc_queue = new_node;
    }
    else{
      struct victim_block* sear_node = recent_gc_queue;

      while(sear_node->next != NULL)
	sear_node = sear_node->next;

      sear_node->next = new_node;
    }

    // Remove Victim block information when GC is done.
    if(remove_node->next != NULL){
      vll->head = remove_node->next;
      remove_node->next = NULL;
      free(remove_node);
      remove_node = NULL;
    }
    else{
      vll->head = NULL;
      vll->tail = NULL;
      free(remove_node);
      remove_node = NULL;
    }

    num_of_vic_blk--;
  }
  else
    printf("Error Non Delete Gc is done! \n");
}

short check_queue_state(){
  int _do = 0;

  if(read_queue != NULL)
    _do = 1;

  return _do;
}

void garbage_collection(struct victim_block* victim_blk, short ch, short mode){ // mode -> 0 normal , mode -> 1 SSD is full
  short _do = 0;
  int i; 

  _do = check_queue_state(); // Check queue is NULL(=EMPTY)

  if(mode == 0){
    if(_do){
      if(read_queue->arrive_time < nand_done_time[ch].done_time)
	return;
    }
  }
   
  if(victim_blk->victim_time <= global_time){
    if(blk_st_tb[ch][victim_blk->plne][victim_blk->blck].gc_on || mode){
      if(victim_blk->cp_done == 1 || mode){
	// Initializate victim block. (GC)
	for(i=0;i<PAGE;i++){
	  blk_tb[ch][victim_blk->plne][victim_blk->blck][i].prev_page.valid = 0;
	  blk_tb[ch][victim_blk->plne][victim_blk->blck][i].next_page.valid = 0;
	  blk_tb[ch][victim_blk->plne][victim_blk->blck][i].first = 0;
	  blk_tb[ch][victim_blk->plne][victim_blk->blck][i].last = 0;
	
	  if(blk_tb[ch][victim_blk->plne][victim_blk->blck][i].valid == 2) 
	    total_invld_pg--;         
	
	  blk_tb[ch][victim_blk->plne][victim_blk->blck][i].valid = 0;           
	}
      
	blk_st_tb[ch][victim_blk->plne][victim_blk->blck].last_invld_time = (DBL_MAX-1);
	blk_st_tb[ch][victim_blk->plne][victim_blk->blck].write_pt = 0;
	blk_st_tb[ch][victim_blk->plne][victim_blk->blck].stream_id = 5;
	blk_st_tb[ch][victim_blk->plne][victim_blk->blck].num_vld_pg = 0;
	blk_st_tb[ch][victim_blk->plne][victim_blk->blck].victim = 0;
      
	nand_done_time[ch].done_time += NAND_E_DELAY;
	nand_done_time[ch].valid = 1;
	total_free_blk++; 
      
	/*if(mode == 0){
	  fprintf(fp_waf, "(Cpsrc) ch:%d | pln:%d | blk:%d | (Cpdes) ch:%d | pln:%d | blk:%d \n", ch, victim_blk->plne, victim_blk->blck, cp_des_blk.ch, cp_des_blk.pln, cp_des_blk.blk);
	  fprintf(fp_waf, "(GC-b) ch:%d | pln:%d | blk:%d | done_time : %.9lf |\n", ch, victim_blk->plne, victim_blk->blck, nand_done_time[ch].done_time);
	  }
	  else{
	  fprintf(fp_waf, "(Cpsrc) ch:%d | pln:%d | blk:%d | (Cpdes) ch:%d | pln:%d | blk:%d \n", ch, victim_blk->plne, victim_blk->blck, cp_des_blk.ch, cp_des_blk.pln, cp_des_blk.blk);
	  fprintf(fp_waf, "(GC-f) ch:%d | pln:%d | blk:%d | done_time : %.9lf |\n", ch, victim_blk->plne, victim_blk->blck, nand_done_time[ch].done_time);
	  }*/
	
	victim_blk->erase_done = 1;
	gc_try++;
	delete_vll(victim_blk, mode);
      }
      else
	printf("Cp is not done! \n");
    }      
  }
  
  return;
}

void search_cp_des_blk(struct physic_addr* des_blk){ // Search Copy-back destionation block.
  unsigned i = 0;

  while(blk_st_tb[i/PLANE/BLOCK%CHANNEL][i/BLOCK%PLANE][i%BLOCK].write_pt != 0){
    i++;
  }

  des_blk->ch = i/PLANE/BLOCK%CHANNEL;
  des_blk->pln = i/BLOCK%PLANE;
  des_blk->blk = i%BLOCK;
  des_blk->valid = 1;   
  total_free_blk--;  

  return;
}

void gc_copy_back(struct victim_block* victim_info, short ch){
  struct physic_addr cp_des;
  unsigned lpn = 0;
  int i = 0;

  if(victim_info->victim_time <= global_time && victim_info->cp_done == 0){
    if(blk_st_tb[ch][victim_info->plne][victim_info->blck].gc_on){
      if(victim_info->cp_done == 0){
	// Search Copy Destination Block
	search_cp_des_blk(&cp_des);

	// Start Copy Back
	while(i < PAGE){
	  if(blk_tb[ch][victim_info->plne][victim_info->blck][i].valid == 1){
	    lpn = blk_tb[ch][victim_info->plne][victim_info->blck][i].lpn;
	    cp_des.pg = blk_st_tb[cp_des.ch][cp_des.pln][cp_des.blk].write_pt;     // copy back destination block's page number.
    
	    /* Move data from victim to copy destination */
	    lpn_ppn_map_tb[lpn] = cp_des;
	    blk_tb[cp_des.ch][cp_des.pln][cp_des.blk][cp_des.pg].lpn = lpn;
	    blk_tb[cp_des.ch][cp_des.pln][cp_des.blk][cp_des.pg].prev_page = blk_tb[ch][victim_info->plne][victim_info->blck][i].prev_page;
	    blk_tb[cp_des.ch][cp_des.pln][cp_des.blk][cp_des.pg].next_page = blk_tb[ch][victim_info->plne][victim_info->blck][i].next_page;
	    blk_tb[cp_des.ch][cp_des.pln][cp_des.blk][cp_des.pg].first = blk_tb[ch][victim_info->plne][victim_info->blck][i].first;
	    blk_tb[cp_des.ch][cp_des.pln][cp_des.blk][cp_des.pg].last = blk_tb[ch][victim_info->plne][victim_info->blck][i].last;
	    blk_tb[cp_des.ch][cp_des.pln][cp_des.blk][cp_des.pg].valid = 1;
	    blk_st_tb_update(cp_des);
    
	    num_of_cp_cnt++;
    
	    if(nand_done_time[cp_des.ch].done_time == 0){
	      nand_done_time[cp_des.ch].done_time = global_time + NAND_W_DELAY;  
	      nand_done_time[cp_des.ch].valid = 1;
	    }
	    else{
	      nand_done_time[cp_des.ch].done_time += NAND_W_DELAY;  
	      nand_done_time[cp_des.ch].valid = 1;
	    }
	  }
	  i++;
	}
	victim_info->cp_done = 1;
	//victim_info->victim_time += nand_done_time[cp_des.ch].done_time;
      }
    }
  }
}

void delete_gc_queue(struct victim_block* removeNode){
  if(recent_gc_queue == removeNode){ // head node
    if(removeNode->next == NULL){ // only exist
      free(removeNode);
      recent_gc_queue = NULL;
      removeNode = NULL;
    }
    else{
      recent_gc_queue = removeNode->next;
      free(removeNode);
      removeNode = NULL;
    }
  }
  else{
    if(removeNode->next == NULL){ //tail node
      struct victim_block* sear_node = recent_gc_queue;
      
      while(sear_node->next != removeNode)
	sear_node = sear_node->next;

      sear_node->next = NULL;
      free(removeNode);
      removeNode = NULL;
    }
    else{  // normal node
      struct victim_block* sear_node = recent_gc_queue;
      
      while(sear_node->next != removeNode)
	sear_node = sear_node->next;

      sear_node->next = removeNode->next;
      removeNode->next = NULL;
      free(removeNode);
      removeNode = NULL;
    }
  }
}
