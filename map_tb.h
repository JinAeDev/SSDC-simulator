#ifndef _MAP_TB_H_
#define _MAP_TB_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// SSD Configuration
#define CHANNEL    4
#define PLANE      32
#define BLOCK      512
#define PAGE       128

// Number of Hashkeys
#define NUM_OF_HASHKEY 4

// Number of Streams
#define NUM_OF_STREAMS 4

//Number of LPN (LPN == PPN 4*32*512*128 = 8,388,608 )
#define NUM_OF_LPN 8388608

// Array of LPN POOL we can know how many lpns are allocated.
int lpn_pool[NUM_OF_LPN];

// la2lpn linked list
struct la2lpn_node {
  unsigned int logic_addr; // First logical address number of task
  unsigned int lpn;
  short        single; // 1 : single, 0 : non-single
  struct la2lpn_node* next;
  struct la2lpn_node* prev;
};

struct la2lpn_node* tb_la2lpn[NUM_OF_HASHKEY];

struct la2lpn_node* search_la2lpn_node(unsigned la);
void create_la2lpn_node(unsigned logic_addr, unsigned lpn, short single);
void create_lpn_ll(unsigned lpn, short first, short last);
void delete_la2lpn(struct la2lpn_node* delete_node, unsigned hash_key);

// lpn linked list
struct lpn_node {
  unsigned int lpn;
  short        first;
  short        last;
  struct lpn_node* next;
  struct lpn_node* prev;
};

struct ll_lpn {
  struct lpn_node* head;
  struct lpn_node* tail;
};

struct ll_lpn* lpn_ll;

unsigned int alloc_lpn();  // Allocate LPN
void delete_lpn_node(unsigned lpn);
void change_last_lpn(unsigned head_la, unsigned lpn);

// Physical Address
struct physic_addr {
  int ch;
  int pln;
  int blk;
  int pg;
  short valid;
};

struct physic_addr lpn_ppn_map_tb[NUM_OF_LPN];
struct physic_addr prev_ppn;
struct physic_addr alloc_ppn(short ch_num, short stream_id);
struct physic_addr search_ppn(unsigned la, int size);  // Read
void phys_addr_invalid(unsigned lpn);

struct access_pt{
  unsigned plne;
  unsigned blck;   
};

struct access_pt access_ppn[CHANNEL][NUM_OF_STREAMS];

int hash_func(unsigned logic_addr);

/*** Block Table Configuration ***/
struct blk_tb_entry{
  struct physic_addr prev_page;
  struct physic_addr next_page;
  unsigned           lpn;
  int                valid;
  int 	             first;
  int 	             last;
};

struct blk_tb_entry blk_tb[CHANNEL][PLANE][BLOCK][PAGE];

void blk_tb_update(struct physic_addr ppn);

/*** Block Status Table Configuration ***/
struct row_blk_stat_tb{
  double last_invld_time;
  int num_vld_pg;
  int write_pt;
  short stream_id;
  short victim;   // If block is selected victim block, "victim" is 1 otherwise 0.
  short gc_on;    
};

struct row_blk_stat_tb blk_st_tb[CHANNEL][PLANE][BLOCK];

void blk_stat_tb_initialize();
void blk_st_tb_update(struct physic_addr ppn);
void blk_st_tb_invld(struct physic_addr invld_ppn);

/*** LA -linked list (head_la - tail_la) ***/
struct la_node{
  unsigned head_la;
  unsigned tail_la;
  struct la_node* next;
  struct la_node* prev;
};

struct la_linked_list{
  struct la_node* head;
  struct la_node* tail;
};

struct la_linked_list* la_ll;

unsigned search_head(unsigned tail_la);
void     create_la_ll(unsigned head_la, unsigned tail_la);
void     delete_la_ll(unsigned head_la, int hashkey);

#endif

