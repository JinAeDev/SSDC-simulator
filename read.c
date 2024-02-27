#include "map_tb.h"
#include "task.h"
#include "gc.h"

void create_read_queue(unsigned logic_addr, int size, double arrive_time, int pid){
  struct read_node* new_node = (struct read_node*)malloc(sizeof(struct read_node));

  new_node->logic_addr = logic_addr;
  new_node->arrive_time = arrive_time;
  new_node->size = size;
  new_node->pid = pid;
  new_node->prev = NULL;
  new_node->next = NULL;

  if(size >= SIZE_THRESHOLD){  // seq
    new_node->seq_or_rand = 's';
  }
  else{ // rand
    new_node->seq_or_rand = 'r';
  }

  if(read_queue == NULL && read_q_tail == NULL){
    read_queue = read_q_tail = new_node;      
  }
  else{      
    read_q_tail->next = new_node;
    new_node->prev = read_q_tail;
    read_q_tail = read_q_tail->next;
  }

  return;
}

void process_read() {
  double r_latency = 0;
  int ch_num[CHANNEL] = {0, 0, 0, 0};
  unsigned i = 0;  
  int8_t read = 0;

  if(read_queue != NULL){
    if(read_queue->arrive_time <= global_time){

      read = check_read_possible(read_queue->logic_addr, ch_num);

      if(read == 1){
	for(i=0;i<CHANNEL;i++){
	  if(ch_num[i]>0){
	    if(read_queue->arrive_time <= nand_done_time[i].done_time){            
	      nand_done_time[i].done_time = nand_done_time[i].done_time + (NAND_R_DELAY*ch_num[i]);
	      nand_done_time[i].valid = 1;
	    }
	    else{
	      nand_done_time[i].done_time = read_queue->arrive_time + NAND_R_DELAY;
	      nand_done_time[i].valid = 1;
	    }
    
	    r_latency = nand_done_time[i].done_time - (read_queue->arrive_time);
	  }
	}
	fprintf(fp_latency,"R,%d,%u,%d,%.9lf,%.9lf,%d,%c\n", ch_num[i], read_queue->logic_addr, read_queue->size, read_queue->arrive_time, r_latency, read_queue->pid ,read_queue->seq_or_rand);
	
	delete_readq();
	
	if(read_queue == NULL)
	  return;         
      }
      else if(read == 2){ // error flag
	int compare = 0;
	int8_t sel_ch = 0;

	for(i=0;i<CHANNEL;i++){
	  if(compare < fmcq_pt[i]){
	    compare = fmcq_pt[i];
	    sel_ch = i;
	  }
	}

	if(read_queue->arrive_time <= nand_done_time[sel_ch].done_time){            
	  nand_done_time[sel_ch].done_time = nand_done_time[sel_ch].done_time + (NAND_R_DELAY*read_queue->size/4096);
	  nand_done_time[sel_ch].valid = 1;
	}
	else{
	  nand_done_time[sel_ch].done_time = read_queue->arrive_time + (NAND_R_DELAY*read_queue->size/4096);
	  nand_done_time[sel_ch].valid = 1;
	}

	r_latency = nand_done_time[sel_ch].done_time - (read_queue->arrive_time);
	fprintf(fp_latency,"R,%d,%u,%d,%.9lf,%.9lf,%d,%c\n", sel_ch, read_queue->logic_addr, read_queue->size, read_queue->arrive_time, r_latency, read_queue->pid, read_queue->seq_or_rand);
	
	delete_readq();
	
	if(read_queue == NULL)
	  return;         
      }
      else{  // read == 0
	return;
      }
    }    

  }
   
}

void delete_readq(){
  if(read_queue->prev == NULL && read_queue->next == NULL){ // only
    free(read_queue);
    read_queue = NULL;
    read_q_tail = NULL;
  }
  else{       
    read_queue = read_queue->next;      
    free(read_queue->prev);   
    read_queue->prev = NULL;   
  }

  return;
}

// possible : 0,  impossible : 1, possible :  2->error_flag
int8_t check_read_possible(unsigned logic_addr, int* ch_num){
  struct la2lpn_node* search_node = NULL;
  struct physic_addr temp;
  unsigned first_lpn = 0;
  int8_t possible = 0;
  int hash_key = hash_func(logic_addr);
  
  search_node = tb_la2lpn[hash_key];
  
  while(search_node != NULL){  // Search mapped lpn 
    if(search_node->logic_addr == logic_addr){
      first_lpn = search_node->lpn;
      break;
    }
    search_node = search_node->next;
  }

  if(search_node == NULL){
    possible = 2;
    return possible;
  }

  if(search_node->single == 0){  // Multi-page file
    struct lpn_node* sear_node = lpn_ll->head;
    
    while(sear_node != NULL){
      if(sear_node->lpn == first_lpn){
	do{
	  temp = lpn_ppn_map_tb[sear_node->lpn];

	  if(blk_st_tb[temp.ch][temp.pln][temp.blk].gc_on == 1 && temp.valid == 1){
	    possible = 0;
	    return possible;
	  }
	  else if(blk_st_tb[temp.ch][temp.pln][temp.blk].gc_on == 0 && temp.valid == 1){
	    possible = 1;
	    (*(ch_num + temp.ch))++;
	  }
	  else
	    possible = 2;

	  sear_node = sear_node->next;

	  if(sear_node == NULL){
	    if(possible == 0){
	      possible = 2;
	      break;
	    }
	    else
	      break;
	  }
	}while(sear_node->first != 1);
	
	if(possible)
	  return possible;
	else
	  break;
      }
      sear_node = sear_node->next;
    }

    if(sear_node == NULL)
      possible = 2;
  }
  else{
    temp = lpn_ppn_map_tb[first_lpn];
    
    if(blk_st_tb[temp.ch][temp.pln][temp.blk].gc_on == 1 && temp.valid == 1)
      possible = 0;
    else if(blk_st_tb[temp.ch][temp.pln][temp.blk].gc_on == 0 && temp.valid == 1){
      possible = 1;
      (*(ch_num+temp.ch))++;
    }
    else
      possible = 2;
  }

  return possible;
}
