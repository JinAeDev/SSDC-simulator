#include "map_tb.h"
#include "task.h"
#include "gc.h"

void insert_task_queue(unsigned int logic_addr, char* iotype, double arrive_time, int size, unsigned cnt, char* data_type, int pid) {
  short stream_id = 0;

  if(queue_size == 1)
    pcieq = (struct task*)malloc(sizeof(struct task));
  else
    pcieq = (struct task*)realloc(pcieq, (sizeof(struct task)*queue_size)); // allocate task queue

  if(cnt == 0){
    (pcieq+(queue_size-1))->first = 1;
    if(size == 4096) (pcieq+(queue_size-1))->last  = 1;
    else (pcieq+(queue_size-1))->last  = 0;
  }
  else{
    if(cnt == (size/4096 - 1)){
      (pcieq+(queue_size-1))->first = 0;
      (pcieq+(queue_size-1))->last  = 1;
    }
    else{
      (pcieq+(queue_size-1))->first = 0;
      (pcieq+(queue_size-1))->last  = 0;
    }
  }

  if(iotype[1] == 'A')
    (pcieq+(queue_size-1))->wa = 1;
  else
    (pcieq+(queue_size-1))->wa = 0;

  if(size >= SIZE_THRESHOLD){ // Seq 
    (pcieq+(queue_size-1))->seq_or_rand = 's';
  }
  else{ // Random
    (pcieq+(queue_size-1))->seq_or_rand = 'r';
  }

  stream_id = alloc_stream_id(data_type);

  (pcieq+(queue_size-1))->head_la     = logic_addr;
  (pcieq+(queue_size-1))->tail_la     = logic_addr + (size/4096 - 1);
  (pcieq+(queue_size-1))->stream_id   = stream_id;
  (pcieq+(queue_size-1))->arrive_time = arrive_time;  // In order to calculate latency
  (pcieq+(queue_size-1))->task_time   = arrive_time;  // Active time
  (pcieq+(queue_size-1))->size        = size;
  (pcieq+(queue_size-1))->valid       = 1;
  (pcieq+(queue_size-1))->pid         = pid;
  strcpy((pcieq+(queue_size-1))->type, iotype);
  queue_size++;
}

void write_data_pcie_to_sram(){
  struct task* task = NULL;

  while((pcieq+pcieq_pt)->task_time <= global_time){     
    check_over_write();      // Check if the task is over-write and proceed invalid-update.
    task = process_pcieq();    
    map_la_to_lpn(task);     // write a page into SRAM, give it a task info and map la to lpn.

    if(task == NULL)
      break;
  }
}

void write_data_sram_to_fmc(){
  struct physic_addr ppn;   
  short              channel = 0; 

  while((sramq+sramq_pt)->task_time <= global_time){
    channel = assign_ch_num();           // Assign channel number before sending to FMC

    if(channel >=  0){
      ppn = alloc_ppn(channel, (sramq+sramq_pt)->stream_id);            // Before writing page into FMC, Map ppn

      if(ppn.valid == 1)
	pending_task[ppn.ch][ppn.pln][ppn.blk]++;

      blk_tb_update(ppn);      
      process_sramq(ppn);
    }
    else
      break;

    if(ppn.valid == 0)
      break;
  }
}

void alloc_task_queue() { // Allocate queue size as many as number of tasks.
  unsigned i, j;
   
  pcieq = (struct task*)realloc(pcieq, (sizeof(struct task)*queue_size+1));
  sramq = (struct task*)malloc(sizeof(struct task)*queue_size+1);

  // Clear validate of queue
  for(i=0;i<queue_size+1;i++){
    (sramq + i)->valid = 0;      
  }

  for(i=0;i<CHANNEL;i++){
    for(j=0;j<PLANE*BLOCK*PAGE;j++){
      ((*(fmcq +i))+j)->valid = 0;
    }
  }
}

short assign_ch_num() {
  short ch_num = -1;

  if((sramq+sramq_pt)->valid == 1 && (sramq+sramq_pt)->task_time <= global_time) {      
    ch_num = rand() % CHANNEL;

    /***  If the current state of "FMC" applies to any of the following conditions, select the channel again.
	  1. The channel is in Garbage collection.
	  2. The FMC buffer is full.
	  3. The fmc buffer is currently in writing. ***/

    if(fmc_port_busy[ch_num] == 1){
      ch_num = -1;
    }

    if(fmc_buff[ch_num] >= 2){
      ch_num = -1;
    }

    return ch_num;      
  }
  else
    return ch_num;
}

double update_global_time() {
  double compare_time = (DBL_MAX - 1);
  int i;
   
  if(sram_done_time.valid == 1 && sram_done_time.done_time < compare_time)
    compare_time = sram_done_time.done_time; 
    
  for(i=0; i<CHANNEL; i++){
    if(fmc_done_time[i].valid == 1 && fmc_done_time[i].done_time < compare_time)
      compare_time = fmc_done_time[i].done_time;
   
    if(nand_done_time[i].valid == 1 && nand_done_time[i].done_time < compare_time){
      compare_time = nand_done_time[i].done_time;
    }
  }

  if(compare_time == (DBL_MAX - 1)){ // ALL buffers are empty in SSD.
    if((pcieq + pcieq_pt)->valid == 1)
      compare_time = (pcieq + pcieq_pt)->task_time;
      
    if(delete_queue != NULL){
      if((compare_time == (DBL_MAX-1)) || (delete_queue->arrive_time < compare_time)){
	compare_time = delete_queue->arrive_time;
      }
    }

    if(read_queue != NULL){
      if((compare_time == (DBL_MAX-1)) || (read_queue->arrive_time < compare_time))
	compare_time = read_queue->arrive_time;
    }
  }  
   
  return compare_time;
}

struct task* process_pcieq() { // Write a task to sram
  if((pcieq+pcieq_pt)->valid == 1 && (pcieq+pcieq_pt)->task_time <= global_time) {
    if(sram_wport_busy == 0) {      
      // Insert task to sram queue
      (sramq+pcieq_pt)->head_la       = (pcieq+pcieq_pt)->head_la;
      (sramq+pcieq_pt)->tail_la       = (pcieq+pcieq_pt)->tail_la;
      (sramq+pcieq_pt)->stream_id     = (pcieq+pcieq_pt)->stream_id;
      (sramq+pcieq_pt)->seq_or_rand   = (pcieq+pcieq_pt)->seq_or_rand;
      (sramq+pcieq_pt)->arrive_time   = (pcieq+pcieq_pt)->arrive_time;
      (sramq+pcieq_pt)->size          = (pcieq+pcieq_pt)->size;  
      (sramq+pcieq_pt)->first         = (pcieq+pcieq_pt)->first; 
      (sramq+pcieq_pt)->last          = (pcieq+pcieq_pt)->last;      
      (sramq+pcieq_pt)->pid           = (pcieq+pcieq_pt)->pid;      
      (sramq+pcieq_pt)->valid         = 1;
      strcpy((sramq+pcieq_pt)->type, (pcieq+pcieq_pt)->type);

      if(pcieq_pt == 0){ // first task in
	sram_done_time.done_time = (pcieq+pcieq_pt)->task_time + SRAM_W_DELAY;
	(sramq+pcieq_pt)->task_time = sram_done_time.done_time;            
      }
      else if(sram_done_time.done_time < (pcieq+pcieq_pt)->task_time){
	sram_done_time.done_time = (pcieq+pcieq_pt)->task_time + SRAM_W_DELAY; 
	(sramq+pcieq_pt)->task_time = sram_done_time.done_time;
      }
      else{
	sram_done_time.done_time += SRAM_W_DELAY;
	(sramq+pcieq_pt)->task_time = sram_done_time.done_time;            
      }

      // Remove task from pcie queue
      (pcieq+pcieq_pt)->valid = 0; 
                 
      sram_done_time.valid = 1;
      sram_wport_busy = 1;     

      // printf("sram : %.9lf(s) \n",sram_done_time.done_time);
      //fprintf(fp_time,"(W)LBA : %u | first : %d | last : %d | sram : %.9lf(s) \n",(pcieq+pcieq_pt)->head_la, (pcieq+pcieq_pt)->first, (pcieq+pcieq_pt)->last, sram_done_time.done_time); 
      pcieq_pt++;
      buff_task++;
      //free( (pcieq+pcieq_pt) );

      return (sramq + (pcieq_pt-1));         
    }
    else
      return NULL;      
  }
  else{
    //sram_done_time.valid = 0;
    return NULL;
  }   
}

void process_sramq(struct physic_addr ppn) { // Write a task from sram to fmc
  if(ppn.valid == 1){
    if((sramq+sramq_pt)->valid == 1 && (sramq+sramq_pt)->task_time <= global_time) {
      if(fmc_port_busy[ppn.ch] == 0 && fmc_buff[ppn.ch] < 2) { 
	// Insert task to sram queue
	(fmcq[ppn.ch] + qp[ppn.ch])->head_la       = (sramq+sramq_pt)->head_la;
	(fmcq[ppn.ch] + qp[ppn.ch])->tail_la       = (sramq+sramq_pt)->tail_la;
	(fmcq[ppn.ch] + qp[ppn.ch])->stream_id     = (sramq+sramq_pt)->stream_id;
	(fmcq[ppn.ch] + qp[ppn.ch])->arrive_time   = (sramq+sramq_pt)->arrive_time;
	(fmcq[ppn.ch] + qp[ppn.ch])->seq_or_rand   = (sramq+sramq_pt)->seq_or_rand;
	(fmcq[ppn.ch] + qp[ppn.ch])->lpn           = (sramq+sramq_pt)->lpn;
	(fmcq[ppn.ch] + qp[ppn.ch])->size          = (sramq+sramq_pt)->size;
	(fmcq[ppn.ch] + qp[ppn.ch])->first         = (sramq+sramq_pt)->first;
	(fmcq[ppn.ch] + qp[ppn.ch])->last          = (sramq+sramq_pt)->last;
	(fmcq[ppn.ch] + qp[ppn.ch])->pid           = (sramq+sramq_pt)->pid;
	(fmcq[ppn.ch] + qp[ppn.ch])->valid         = 1;
	strcpy((fmcq[ppn.ch] + qp[ppn.ch])->type, (sramq+sramq_pt)->type);


	if(sramq_pt == 0){ // first task in
	  fmc_done_time[ppn.ch].done_time        = (sramq+sramq_pt)->task_time + FMC_W_DELAY;
	  (fmcq[ppn.ch] + qp[ppn.ch])->task_time = fmc_done_time[ppn.ch].done_time;            
	}
	else if(fmc_done_time[ppn.ch].done_time < (sramq+sramq_pt)->task_time){
	  fmc_done_time[ppn.ch].done_time = (sramq+sramq_pt)->task_time + FMC_W_DELAY; 
	  (fmcq[ppn.ch] + qp[ppn.ch])->task_time = fmc_done_time[ppn.ch].done_time;
	}
	else{
	  fmc_done_time[ppn.ch].done_time += FMC_W_DELAY;
	  (fmcq[ppn.ch] + qp[ppn.ch])->task_time = fmc_done_time[ppn.ch].done_time;            
	}
         
	// Remove task from pcie queue
	(sramq+sramq_pt)->valid = 0;            
         
	qp[ppn.ch]++;
	fmc_buff[ppn.ch]++;
	sramq_pt++;
	buff_task--;
         
	fmc_done_time[ppn.ch].valid = 1;
	fmc_port_busy[ppn.ch]       = 1;     

	//printf("fmc[ch:%d] : %.9lf(s) \n", ppn.ch, fmc_done_time[ppn.ch].done_time);
	//fprintf(fp_time,"(W)LBA : %u | first : %d | last : %d | fmc[ch:%d] : %.9lf(s) \n",(sramq+sramq_pt)->head_la, (sramq+sramq_pt)->first, (sramq+sramq_pt)->last, ppn.ch, fmc_done_time[ppn.ch].done_time);
	//free( (sramq+sramq_pt) );
      }       
    }
  }    
}

void write_data_fmc_to_nand() {
  //double time_buf = (DBL_MAX-1);
  unsigned short ch_num = 0;
  unsigned lpn = 0;
  //int i;
   
  while( ch_num < CHANNEL ){
    while( (fmcq[ch_num] + fmcq_pt[ch_num])->valid == 1 && (fmcq[ch_num] + fmcq_pt[ch_num])->task_time <= global_time){
      if( nand_busy[ch_num] == 0  && fmc_buff[ch_num] != 0){
	if( fmcq_pt[ch_num] == 0){ // first task in
	  if(nand_done_time[ch_num].done_time == 0)
	    nand_done_time[ch_num].done_time = (fmcq[ch_num] + fmcq_pt[ch_num])->task_time + NAND_W_DELAY;
	  else
	    nand_done_time[ch_num].done_time += NAND_W_DELAY;                             
	}
	else if(nand_done_time[ch_num].done_time < (fmcq[ch_num] + fmcq_pt[ch_num])->task_time){
	  nand_done_time[ch_num].done_time = (fmcq[ch_num] + fmcq_pt[ch_num])->task_time + NAND_W_DELAY;                
	}
	else{
	  nand_done_time[ch_num].done_time += NAND_W_DELAY;                           
	}
     
	nand_done_time[ch_num].valid = 1;
	num_of_write_task++; 
	fmc_buff[ch_num]--;
	nand_busy[ch_num] = 1;
	
	lpn = (fmcq[ch_num] + fmcq_pt[ch_num])->lpn;
	pending_task[lpn_ppn_map_tb[lpn].ch][lpn_ppn_map_tb[lpn].pln][lpn_ppn_map_tb[lpn].blk]--;

	if(pending_task[lpn_ppn_map_tb[lpn].ch][lpn_ppn_map_tb[lpn].pln][lpn_ppn_map_tb[lpn].blk] < 0)
	  pending_task[lpn_ppn_map_tb[lpn].ch][lpn_ppn_map_tb[lpn].pln][lpn_ppn_map_tb[lpn].blk] = 0;

	(fmcq[ch_num] + fmcq_pt[ch_num])->valid = 0;
       
	// PRINT Write Latency in text file 
	if((fmcq[ch_num] + fmcq_pt[ch_num])->last == 1){             
	  fprintf(fp_latency,"W,%d,%u,%d,%.9lf,%.9lf,%d,%c\n",ch_num, (fmcq[ch_num] + fmcq_pt[ch_num])->head_la, (fmcq[ch_num] + fmcq_pt[ch_num])->size, (fmcq[ch_num] + fmcq_pt[ch_num])->arrive_time ,nand_done_time[ch_num].done_time - (fmcq[ch_num] + fmcq_pt[ch_num])->arrive_time, (fmcq[ch_num] + fmcq_pt[ch_num])->pid, (fmcq[ch_num] + fmcq_pt[ch_num])->seq_or_rand);
	}

	fmcq_pt[ch_num]++; 
      }
      else
	break;
    }
    ch_num++;
  }
}

short check_write_done(){
  static unsigned loop_cnt = 0;
  unsigned i = 0;
  short    all_task_done = 1;

  if(loop_cnt % 100000 == 0){ // Print Result to text file
    printf("----------------------------------------------------------------------------------------------------\n");
    fprintf(fp_waf,"----------------------------------------------------------------------------------------------------\n");
    printf("Loop Count : %u | GC try ; %u\n", loop_cnt, gc_try);
    fprintf(fp_waf,"Loop Count : %u | GC try ; %u\n", loop_cnt, gc_try);
    printf("global_time : %.9lf | pcieq_pt : %u | num_of_write_task : %u | buff_task : %u \n", global_time, pcieq_pt, num_of_write_task, buff_task);
    fprintf(fp_waf,"global_time : %.9lf | pcieq_pt : %u | num_of_write_task : %u | buff_task : %u \n", global_time, pcieq_pt, num_of_write_task, buff_task);
    printf("CP_cnt : %u, WAF : %.9lf \n",num_of_cp_cnt, (double)(num_of_write_task + num_of_cp_cnt)/num_of_write_task);
    fprintf(fp_waf,"CP_cnt : %u, WAF : %.9lf \n",num_of_cp_cnt, (double)(num_of_write_task + num_of_cp_cnt)/num_of_write_task);
    printf("Num of victim block : %d \n", num_of_vic_blk);
    fprintf(fp_waf,"Num of victim block : %d \n", num_of_vic_blk);
    printf("----------------------------------------------------------------------------------------------------\n");
    fprintf(fp_waf,"----------------------------------------------------------------------------------------------------\n");
  }

  if((pcieq + pcieq_pt)->valid == 1) all_task_done = 0;
  if((sramq + sramq_pt)->valid == 1) all_task_done = 0;
  if( read_queue != NULL ) all_task_done = 0;

  while(i < CHANNEL){
    if((fmcq[i] + fmcq_pt[i])->valid == 1) all_task_done = 0;
    if( nand_busy[i] == 1) all_task_done = 0;
    if(delete_com == 1) all_task_done = 0;
    i++;
  } 

  loop_cnt++;     

  return all_task_done;
}

