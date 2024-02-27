#include "map_tb.h"
#include "task.h"
#include "gc.h"

unsigned int alloc_lpn() {
  unsigned int alloc_lpn = 0;  

  while(lpn_pool[alloc_lpn] == 1)
    alloc_lpn++;

  lpn_pool[alloc_lpn] = 1;

  return alloc_lpn;
}

void map_la_to_lpn(struct task* task) {
  if(task == NULL) return;

  if(task->first != 1)
    return;
  else{
    create_la_ll(task->head_la, task->tail_la);

    unsigned lpn = 0;    
    short    single = 0;
    int      page_cnt = task->size / 4096;
    int      i;      
           
    for(i=0; i<page_cnt; i++){
      lpn = alloc_lpn();
          
      if((sramq + (pcieq_pt-1) + i)->wa == 0)
	(sramq + (pcieq_pt-1) + i)->lpn = lpn;
      else // wa
	change_last_lpn(task->head_la, lpn);

      if(page_cnt == 1){  // Single size task     
	single = 1;
	create_la2lpn_node(task->head_la, lpn, single);
      }         
      else{  // Multi size task
	if(i == 0){ // first node
	  create_la2lpn_node(task->head_la, lpn, single);
	  create_lpn_ll(lpn, 1, 0);              // create_lpn_ll(lpn, first, last) --> if the lpn is first lpn, first <- 1.
	}
	else if(i == (page_cnt - 1)){ // last node
	  create_lpn_ll(lpn, 0, 1);
	}
	else{ // normal node
	  create_lpn_ll(lpn, 0, 0);
	}
      }       
    }
  }           
}

void create_la2lpn_node(unsigned logic_addr, unsigned lpn, short single) {
  struct la2lpn_node* new_node = (struct la2lpn_node*)malloc(sizeof(struct la2lpn_node));
  int hashkey = hash_func(logic_addr);

  new_node->logic_addr = logic_addr;
  new_node->lpn        = lpn;
  new_node->single     = single;
  new_node->prev       = NULL;
  new_node->next       = NULL;

  if(tb_la2lpn[hashkey] == NULL) {
    tb_la2lpn[hashkey] = new_node;
  }
  else{
    struct la2lpn_node* search_node = tb_la2lpn[hashkey];
     
    while(search_node->next != NULL)
      search_node = search_node->next;

    search_node->next = new_node;
    new_node->prev = search_node;
  }
   
  //printf("<LA2LPN> hashkey : %d (lba : %u - lpn : %u , single : %d) \n\n",hashkey, logic_addr, lpn, single);
  //fprintf(fp_la2lpn,"<LA2LPN> hashkey : %d (lba : %u - lpn : %u , single : %d) | time : %.9lf \n\n",hashkey, logic_addr, lpn, single, global_time);
}

void create_lpn_ll(unsigned lpn, short first, short last) {
  struct lpn_node* new_node = (struct lpn_node*)malloc(sizeof(struct lpn_node));

  new_node->lpn   = lpn;
  new_node->first = first;
  new_node->last  = last;
  new_node->prev  = NULL;
  new_node->next  = NULL;

  if(lpn_ll->head == NULL && lpn_ll->tail == NULL) {
    lpn_ll->head = lpn_ll->tail = new_node;      
  }
  else{
    lpn_ll->tail->next = new_node;
    new_node->prev = lpn_ll->tail;
    lpn_ll->tail = lpn_ll->tail->next;
  }

  //printf("<LPN_ll> (lpn : %u - first : %d , last : %d) \n\n", lpn, first, last);
  //fprintf(fp_la2lpn,"<LPN_ll> (lpn : %u - first : %d , last : %d) | time : %.9lf \n\n", lpn, first, last, global_time);
}

void check_over_write(){
  struct la2lpn_node* search_node = NULL;
  struct physic_addr over_ppn;
  unsigned logic_addr = 0;
  unsigned hash_key   = 0;
      
  if((pcieq+pcieq_pt)->valid == 1 && (pcieq+pcieq_pt)->task_time <= global_time) {
    if( (pcieq+pcieq_pt)->first == 1) {
      if( (pcieq+pcieq_pt)->type[1] != 'A' ){ // over-write
	logic_addr = (pcieq+pcieq_pt)->head_la;
	hash_key = hash_func(logic_addr);

	search_node = tb_la2lpn[hash_key];

	while(search_node != NULL){
	  if(search_node->logic_addr == logic_addr){
	    if(search_node->single == 1){
	      over_ppn = lpn_ppn_map_tb[search_node->lpn];

	      if(blk_st_tb[over_ppn.ch][over_ppn.pln][over_ppn.blk].gc_on != 1){ // Check GC
		delete_la_ll(search_node->logic_addr, hash_key);
		delete_la2lpn(search_node, hash_key);
		break;
	      }
	      else{  // Do not now
		return;
	      }
	    }
	    else{  // It is not single page
	      // Search Lpns
	      struct lpn_node* sear_node = NULL;
	      
	      while(sear_node != NULL){
		if(sear_node->lpn == search_node->lpn) // Find First Lpn Node in Lpn Linked List
		  break;
		else
		  sear_node = sear_node->next;
	      }

	      if(sear_node == NULL){
		delete_la_ll(search_node->logic_addr, hash_key);
		delete_la2lpn(search_node, hash_key);
	      }
	      else{
		do{
		  if(blk_st_tb[lpn_ppn_map_tb[sear_node->lpn].ch][lpn_ppn_map_tb[sear_node->lpn].pln][lpn_ppn_map_tb[sear_node->lpn].blk].gc_on == 1){
		    return;
		  }

		  sear_node = sear_node->next;
		}while(sear_node->last != 1);

		delete_la_ll(search_node->logic_addr, hash_key);
		delete_la2lpn(search_node, hash_key);
		break;
	      }	      
	    }	    
	  }            
	  else
	    search_node = search_node->next;
	}
	if(search_node == NULL) return;
      }
      else{ // WA request
	request_write_append((pcieq+pcieq_pt)->tail_la);  // -ing
      }
    }
  }
}

void request_write_append(unsigned tail_la){
  unsigned head_la = search_head(tail_la);
   
  if(head_la == 0)
    return; 

  struct la2lpn_node* search_node = search_la2lpn_node(head_la);   

  if(search_node->single == 0){
    struct lpn_node* sear_node = lpn_ll->head;

    while(sear_node != NULL){
      if(sear_node->lpn == search_node->lpn)
	break;   
      
      sear_node = sear_node->next;
    }
      
    if(sear_node == NULL){ // pre-invalidated data
      return;
    }

    while(sear_node->last != 1){
      sear_node = sear_node->next;

      if(sear_node == NULL)
	break;
    }

    if(sear_node == NULL){ // pre-invalidated data
      return;
    }
      
    phys_addr_invalid(sear_node->lpn);      
  }
  else{
    phys_addr_invalid(search_node->lpn);      
  } 
}

void delete_la2lpn(struct la2lpn_node* delete_node, unsigned hash_key){
  unsigned lpn = delete_node->lpn;

  /*** LPN NODE DELETE and Invalid update block tb ***/
  if(delete_node->single == 0){
    delete_lpn_node(lpn);
  }
  else{
    //if(lpn_ppn_map_tb[lpn].valid == 1)
    phys_addr_invalid(lpn);
  } 

  /*** LA2LPN NODE DELETE ***/
  if(delete_node->prev == NULL){ // head_node
    if(delete_node->next == NULL){
      tb_la2lpn[hash_key] = NULL;
      free(delete_node); 
      delete_node = NULL;
    }
    else{
      tb_la2lpn[hash_key] = delete_node->next;
      tb_la2lpn[hash_key]->prev = NULL;     
      free(delete_node);
      delete_node = NULL;
    }
    return;
  }
  else if(delete_node->prev != NULL && delete_node->next == NULL){ // tail node
    delete_node->prev->next = NULL;
    delete_node->prev = NULL;
    free(delete_node);
    delete_node = NULL;
    return;
  }
  else{ // normal node
    delete_node->prev->next = delete_node->next;
    delete_node->next->prev = delete_node->prev;
    delete_node->prev = delete_node->next = NULL;
    free(delete_node);
    delete_node = NULL;
    return;
  }
}

void delete_lpn_node(unsigned lpn){
  struct lpn_node* searchNode = lpn_ll->head;
  struct lpn_node* removeNode = NULL;
   
  while(searchNode != NULL){
    if(searchNode->lpn == lpn){
      do{
	// physical address invalid
	//if(lpn_ppn_map_tb[lpn].valid == 1)
	phys_addr_invalid(searchNode->lpn);

	removeNode = searchNode;

	if (removeNode->prev != NULL) 
	  removeNode->prev->next = removeNode->next;
	else // head update
	  lpn_ll->head = removeNode->next;

	if(removeNode->next != NULL)
	  removeNode->next->prev = removeNode->prev;
	else // tail_update
	  lpn_ll->tail = removeNode->prev;                 

	free(removeNode); 
	removeNode = NULL;
	searchNode = searchNode->next;

	if(searchNode == NULL)
	  break;					
      }while (searchNode->first != 1);
    }
    else
      searchNode = searchNode->next;
  }  
}

struct la2lpn_node* search_la2lpn_node(unsigned la){
  unsigned hash_key = hash_func(la);
  struct la2lpn_node* search_node = tb_la2lpn[hash_key];

  while(search_node != NULL){
    if(search_node->logic_addr == la)
      break;

    search_node = search_node->next;
  }

  if(search_node == NULL)
    return NULL;
  else
    return search_node;
}

void create_la_ll(unsigned head_la, unsigned tail_la){
  struct la_node* new_node = (struct la_node*)malloc(sizeof(struct la_node));

  new_node->head_la = head_la;
  new_node->tail_la = tail_la;
  new_node->next = NULL;
  new_node->prev = NULL;

  if(la_ll->head == NULL && la_ll->tail == NULL){
    la_ll->head = la_ll->tail = new_node;      
  }
  else{
    la_ll->tail->next = new_node;
    new_node->prev = la_ll->tail;
    la_ll->tail = la_ll->tail->next;
  }
}

void delete_la_ll(unsigned head_la, int hashkey){
  struct la_node* search_node = la_ll->head;

  while(search_node != NULL){
    if(search_node->head_la == head_la)
      break;
    else
      search_node = search_node->next;
  }

  if(search_node == NULL)
    return;
  else{
    if(search_node->prev == NULL){ // head
      if(search_node->next == NULL){ // only exist
	la_ll->head = NULL;
	la_ll->tail = NULL;
	free(search_node);
	search_node = NULL;
      }
      else{
	la_ll->head = search_node->next;
	search_node->next = NULL;
	la_ll->head->prev = NULL;
	free(search_node);
	search_node = NULL;
      }
      return;
    }
    else if(search_node->next == NULL){ // tail
      la_ll->tail = search_node->prev;
      la_ll->tail->next = NULL;
      search_node->prev = NULL;
      free(search_node);
      search_node = NULL;
      return;
    }
    else{
      search_node->prev->next = search_node->next;
      search_node->next->prev = search_node->prev;
      search_node->next = NULL;
      search_node->prev = NULL;
      free(search_node);
      search_node = NULL;
      return;
    }
  }
}

unsigned search_head(unsigned tail_la){
  struct la_node* search_node = la_ll->head;

  while(search_node != NULL){
    if(search_node->tail_la == tail_la)
      break;
    else
      search_node = search_node->next;
  }

  if(search_node == NULL)
    return 0;
  else{
    return search_node->head_la;
  }
}

void change_last_lpn(unsigned head_la, unsigned lpn){
  struct la2lpn_node* search_node = search_la2lpn_node(head_la);
  struct lpn_node* sear_node = lpn_ll->head;

  while(sear_node != NULL){
    if(sear_node->lpn == search_node->lpn)
      break;
    else
      sear_node = sear_node->next;
  }

  while(sear_node->last != 1){
    sear_node = sear_node->next;
  }

  lpn_pool[sear_node->lpn] = 0;
  sear_node->lpn = lpn;
  lpn_pool[lpn] = 1;
}
