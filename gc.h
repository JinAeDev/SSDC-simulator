#ifndef _GC_HEADER_
#define _GC_HEADER_
#define GC_THRESHOLD_BACK 0.6
#define GC_THRESHOLD_FORE 0.4
#define AGED_STORAGE 49152 // used 75%

// Total block in each flash memory of SSD (81920 new)

// Linked list of "Victim Block"
struct victim_block{
  unsigned short chnl;
  unsigned plne;
  unsigned blck;
  double   victim_time;
  short    cp_done;
  short    erase_done;
  struct victim_block* next;   
};

struct victim_block* recent_gc_queue;

struct victim_linked_list{
  struct victim_block* head;
  struct victim_block* tail;
};

struct victim_linked_list* vll;

// Global variables
unsigned total_free_blk;
unsigned total_invld_pg;
int num_of_vic_blk;

// Functions
short check_queue_state();
void garbage_collection(struct victim_block* victim_blk, short ch, short mode); // mode -> 0 (background) , mode -> 1 (foreground)
void vll_update(struct physic_addr invld_ppn);
void insert_vll(struct physic_addr invld_ppn);
void delete_vll(struct victim_block* remove_node, short mode);
void search_cp_des_blk(struct physic_addr* des_blk);
//void vll_jump_insert(int ch, int pln, int blk);
//void insert_min_invld_blk(short ch);
void check_gc_possible();
void delete_gc_queue(struct victim_block* removeNode);
void gc_copy_back(struct victim_block* victim_info, short ch);
struct victim_block* search_victim_blk(struct physic_addr invld_ppn);
#endif
