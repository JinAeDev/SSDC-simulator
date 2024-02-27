#include "map_tb.h"
#include "task.h"
#include "gc.h"

void open_file_io();

int main() {
  // Workload File and print file
  open_file_io();    

  // Clear Time seed to allocate lpn 
  srand((unsigned)time(NULL));

  // Create and Declare linked list & Block table
  create_linked_list();
 	
  // I/O request elements
  short  all_task_done = 0;    // If all incoming tasks are finished, the value is 1 otherwise 0.

  // Initializate global variable   
  num_of_write_task = 0;
  num_of_dump_task  = 0;
  num_of_cp_cnt     = 0;
  buff_task         = 0;
  queue_size        = 1;
  global_time       = 0;   
  pcieq_pt          = 0;
  sramq_pt          = 0;   
  sram_wport_busy   = 0; 
  delete_com        = 0; 
  gc_try            = 0;   
  num_of_vic_blk    = 0;

  /*** Dump tasks to PCIe task queue***/
  blk_stat_tb_initialize();
  dump_workload_task();
  alloc_task_queue();   // Assigns the size of the queue by the number of tasks.  

  // set global time as first task time
  global_time = (pcieq + pcieq_pt)->task_time;
	
  /*** Start simulator ***/
  while (1) {
    // Check that the "Delete" command has been received.
    process_delete();    

    // Check that the "Read" command has been received.
    process_read();      
    
    // Write data from PCIe cont. to SRAM
    write_data_pcie_to_sram();     
    
    // Write data from SRAM_buffer to FMC_buffer
    write_data_sram_to_fmc();      
    
    // Write data from FMC_buffer to NAND
    write_data_fmc_to_nand();
    
    global_time = update_global_time();   // Update global_time

    check_gc_possible();                  // Check that GC can be enabled.
    
    if(vll->head != NULL){
      gc_copy_back(vll->head, vll->head->chnl);
      garbage_collection(vll->head, vll->head->chnl, 0);
    }

    // Clear port status --> Make ports after write-operation not busy.
    clear_port_status();      

    // All of tasks are done. So, break the loop and exit the simulator.
    all_task_done = check_write_done(); 
  
    if(all_task_done)
      break;

  }// end point of main loop.

  fclose(workload);
  fclose(fp_latency);
  fclose(fp_waf);

  return 0;

} // end point of main func.

void open_file_io(){
  int select = 0;

  printf("Select workload\n");
  printf("0) Cassandra   1) Mongodb   2) Mysql   3) Rocksdb   4) Dbench   5) SQLite\n");
  scanf("%d", &select);

  switch(select){
  case 0:
    workload     = fopen("/home/jae/Desktop/spc_workload/cassandra_short.txt","rt");      
    fp_latency   = fopen("/home/jae/Desktop/ver7.0_result/cassandra_short/latency.txt","wt");
    fp_waf       = fopen("/home/jae/Desktop/ver7.0_result/cassandra_short/waf.txt","wt");
    break;
  case 1:
    workload     = fopen("/home/jae/Desktop/spc_workload/mongodb.txt","rt");      
    fp_latency   = fopen("/home/jae/Desktop/ver7.0_result/mongodb/latency.txt","wt");
    fp_waf       = fopen("/home/jae/Desktop/ver7.0_result/mongodb/waf.txt","wt");
    break;
  case 2:
    workload     = fopen("/home/jae/Desktop/spc_workload/mysql.txt","rt");      
    fp_latency   = fopen("/home/jae/Desktop/ver7.0_result/mysql/latency.txt","wt");
    fp_waf       = fopen("/home/jae/Desktop/ver7.0_result/mysql/waf.txt","wt");
    break;
  case 3:
    workload     = fopen("/home/jae/Desktop/spc_workload/rocksdb.txt","rt");      
    fp_latency   = fopen("/home/jae/Desktop/ver7.0_result/rocksdb/latency.txt","wt");
    fp_waf       = fopen("/home/jae/Desktop/ver7.0_result/rocksdb/waf.txt","wt");
    break;
  case 4:
    workload     = fopen("/home/jae/Desktop/spc_workload/dbench.txt","rt");      
    fp_latency   = fopen("/home/jae/Desktop/ver7.0_result/dbench/latency.txt","wt");
    fp_waf       = fopen("/home/jae/Desktop/ver7.0_result/dbench/waf.txt","wt");
    break;
  case 5:
    workload     = fopen("/home/jae/Desktop/spc_workload/sqlite.txt","rt");      
    fp_latency   = fopen("/home/jae/Desktop/ver7.0_result/sqlite/latency.txt","wt");
    fp_waf       = fopen("/home/jae/Desktop/ver7.0_result/sqlite/waf.txt","wt");
    break;
  default:
    printf("You've selected strange workload. Restart simulator! \n");
    exit(1);
    break;
  }       

  fprintf(fp_latency,"CH,LBA,SIZE,Arrive_time(s),Latency(s),pid,seq or rand\n");
  fprintf(fp_latency,"-------------------------------------------------------------\n");

  return;
}


void dump_workload_task() {
  unsigned logic_addr  = 0;
  int      size        = 0;
  double   arrive_time = 0;
  char     iotype[5];
  int      pid         = 0;
  int      inode       = 0;
  char     data_type[30];
  char*    io_request = (char*)malloc(sizeof(char)*100);   
  int      i;
 
  while(1) { // read an I/O reqeust from the workload 
    io_request = fgets(io_request, 100, workload);

    if(io_request == NULL)
      break;
    else{    
      sscanf(io_request, "0,%d,%d,%[^','],%lf,%d,%d,%s", &logic_addr, &size, iotype, &arrive_time, &pid, &inode, data_type);
      
      if(iotype[0] == 'W') { 
	for(i = 0; i < size/4096; i++){  // repeat this loop for each page of IO request
	  /* Insert a page size task into PCIe task queue */
	  insert_task_queue(logic_addr, iotype, arrive_time, size, i, data_type, pid);
	  num_of_dump_task++;          
	}
      }
      else if(iotype[0] == 'D'){
	/* Create delete queue to perform the "Delete-command" */
	create_delete_queue(logic_addr, arrive_time);
      }
      else if(iotype[0] == 'R'){
	/* Create read queue to perform the "Read-command" */
	create_read_queue(logic_addr, size, arrive_time, pid);
      }              
    }
  } 

  return;    
}

void create_linked_list() {
  int i, j;   

  // Initializate Hash table
  for(i = 0; i < NUM_OF_HASHKEY; i++){
    tb_la2lpn[i] = NULL;  
  }

  // Create and declare la_linked_list
  la_ll = (struct la_linked_list*)malloc(sizeof(struct la_linked_list));
  la_ll->head = NULL;
  la_ll->tail = NULL;

  // Create and declare lpn_linked_list;
  lpn_ll = (struct ll_lpn*)malloc(sizeof(struct ll_lpn));
  lpn_ll->head = NULL;
  lpn_ll->tail = NULL; 
  
  // Declare read_queue
  read_queue  = NULL;
  read_q_tail = NULL;

  // Declare delete_queue
  delete_queue  = NULL;
  delete_q_tail = NULL; 
   
  // Declare victim block linked list
  vll = (struct victim_linked_list*)malloc(sizeof(struct victim_linked_list));
  vll->head = vll->tail = NULL;
  recent_gc_queue = NULL;

  total_free_blk  = CHANNEL*PLANE*BLOCK;
  total_invld_pg  = 0; 

  // Initialize control signal of multi-channel FMC and NAND.
  for(i = 0; i < CHANNEL; i++){
    nand_done_time[i].done_time = 0;
    nand_done_time[i].valid = 0;
    fmc_done_time[i].done_time = 0;
    fmc_done_time[i].valid = 0;
    fmc_buff[i]        = 0;
    fmcq_pt[i]         = 0;
    fmc_port_busy[i]   = 0;
    qp[i]              = 0;
    nand_busy[i]       = 0;

    for(j=0;j<NUM_OF_STREAMS;j++){
      access_ppn[i][j].plne = 0;
      access_ppn[i][j].blck = 0;
    }
  } 

  return;
}

void clear_port_status() {  // Make ports after write-operation not busy
  struct victim_block* sear_node = recent_gc_queue;
  int i;

  if(global_time >= sram_done_time.done_time){
    sram_wport_busy = 0;
    sram_done_time.valid = 0;
  }   

  for(i=0;i<CHANNEL;i++){
    if(global_time >= fmc_done_time[i].done_time){
      fmc_port_busy[i]       = 0;
      fmc_done_time[i].valid = 0;
    }

    if(global_time >= nand_done_time[i].done_time){
      nand_busy[i]            = 0;
      nand_done_time[i].valid = 0;
    }
  }

  while(sear_node != NULL){
    if(nand_done_time[sear_node->chnl].done_time <= global_time){
      //if(nand_done_time[sear_node->chnl].done_time < gc_done_time[sear_node->chnl].done_time)
      //nand_done_time[sear_node->chnl].done_time = gc_done_time[sear_node->chnl].done_time;

      if(blk_st_tb[sear_node->chnl][sear_node->plne][sear_node->blck].gc_on == 1){
	blk_st_tb[sear_node->chnl][sear_node->plne][sear_node->blck].gc_on = 0;
	//gc_done_time[sear_node->chnl].done_time = 0;
	//gc_done_time[sear_node->chnl].valid = 0;
	delete_gc_queue(sear_node);
      }
    }
    sear_node = sear_node->next;
  }
  
  return;
}

