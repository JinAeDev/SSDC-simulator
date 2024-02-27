#ifndef _TASK_H_DEFINE_
#define _TASK_H_DEFINE_
#include <limits.h>
#include <float.h>
#include <time.h>
#include <stdint.h>

// Delay Parameters time[s]
#define CH  4
#define PLN 32
#define BLK 512
#define PG 128
#define SRAM_W_DELAY 0.000009 // (5ns / 18bit) * 2^15 = 9us ---> sram write latency
#define SRAM_R_DELAY 0.000015 // (8ns / 18bit) * 2^15 = 15us ---> sram read latency
//#define DRAM_W_DELAY 0.000076 // 15us + (15ns / byte) * 2^12 = 76us ---> (sram read + dram write) delay
#define FMC_W_DELAY    0.000015 // 15us (SRAM_W_DELAY + SRAM_R_DELAY)
#define NAND_W_DELAY 0.00023 // 230us
#define NAND_R_DELAY 0.000025 // 25us
#define NAND_E_DELAY 0.0007 // 0.7ms
#define SIZE_THRESHOLD 16384

// Task queue size
unsigned queue_size; // Allocate task size from number_of_task using with "malloc" function.

// Done time of each buffers
struct done_time_info{
  double done_time;
  int    valid;
};

struct done_time_info sram_done_time; 
struct done_time_info fmc_done_time[CH];
struct done_time_info nand_done_time[CH];

double global_time;  // Criteria time of SSDC Modeling 
double last_fmc_done_time;

// File pointer
FILE* workload;    // To dump spec of workload in dump loop.
FILE* fp_latency;  // To print task write latency
FILE* fp_waf;

// Point each queue array's index number.
unsigned pcieq_pt;           // Queue pointer of pcieq
unsigned sramq_pt;           // Queue Pointer of sramq
unsigned qp[CH];        // Queue pointer to move data to fmc[CHANNEL] from sram
unsigned fmcq_pt[CH];   // Queue Pointer of fmcq each channel.
unsigned num_of_dump_task;   // Number of tasks which are dumped.
unsigned num_of_write_task;  // Number of tasks which are stored in NAND.  
unsigned buff_task;          // Number of task in SRAM buffer currently.
unsigned num_of_cp_cnt;      // Number of tasks which are copied by garbage-collection.
unsigned gc_try;
int pending_task[CH][PLN][BLK];        // Number of tasks which are suspended in Channel.

// Control signal
short sram_wport_busy;
short fmc_write_start;
short fmc_buff[CH];
short fmc_port_busy[CH];
short nand_busy[CH];
short delete_com;

// Write Task structure  // Host Write
struct task {
  unsigned head_la;
  unsigned tail_la;
  unsigned lpn;
  unsigned size;
  double   arrive_time;  // Task's arrive time from host.
  double   task_time;    // Task's operation time.
  char     type[4]; 
  char     seq_or_rand;
  short    stream_id;
  short    first;
  short    last;  
  short    valid; // If a task is not written yet, valid is 1. Otherwise 0.
  short    wa;    // If a task is write-append(WA), the value is 1. Otherwise 0.   
  int      pid;
};

// Task queue (Array)
struct task* pcieq;
struct task* sramq;
struct task  fmcq[CH][PLN*BLK*PG];

struct task* process_pcieq();
short alloc_stream_id(char* data_type);
void map_la_to_lpn(struct task* task);
void process_sramq(struct physic_addr ppn);
void write_data_fmc_to_nand();

// Read-queue (Linked list) // Host_Read
struct read_node{
  unsigned logic_addr;
  double   arrive_time;
  int      size;   
  int      pid;
  char     seq_or_rand;
  struct read_node* next;
  struct read_node* prev;
};

struct read_node* read_queue;
struct read_node* read_q_tail;

// Delete-queue (Linked list)
struct delete_node{
  unsigned logic_addr;
  double   arrive_time;
  struct delete_node* next;
};

struct delete_node* delete_queue;
struct delete_node* delete_q_tail;

// function 
double update_global_time();
short  assign_ch_num();
short  check_write_done();
int8_t check_read_possible(unsigned logic_addr, int* ch_num);
void   check_over_write();
void   insert_task_queue(unsigned int logic_addr, char* iotype, double arrive_time, int size, unsigned cnt, char* data_type, int pid); 
void   create_read_queue(unsigned logic_addr, int size, double arrive_time, int pid);
void   create_delete_queue(unsigned logic_addr, double arrive_time);
void   alloc_task_queue();
void   clear_port_status();
void   create_linked_list();
void   dump_workload_task();
void   process_read();
void   process_delete();
void   delete_readq();
void   request_write_append(unsigned tail_la);
void write_data_pcie_to_sram();
void write_data_sram_to_fmc();
#endif
