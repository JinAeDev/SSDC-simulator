#include "map_tb.h"
#include "task.h"
#include "gc.h"

void create_delete_queue(unsigned logic_addr, double arrive_time){
  struct delete_node* new_node = (struct delete_node*)malloc(sizeof(struct delete_node)); 

  new_node->logic_addr  = logic_addr;
  new_node->arrive_time = arrive_time;
  new_node->next = NULL;

  if(delete_queue == NULL){
    delete_queue = delete_q_tail = new_node;      
  }
  else{
    delete_q_tail->next = new_node;
    delete_q_tail = new_node;
  }
}

void process_delete() {
  if(delete_queue != NULL){
    delete_com = 1;     

    while(delete_queue->arrive_time <= global_time){
      struct la2lpn_node* removeNode = NULL;

      removeNode = search_la2lpn_node(delete_queue->logic_addr);  
     
      if(removeNode == NULL){  // Here including queue delete will
	if(delete_queue->next != NULL){
	  struct delete_node* del_node = delete_queue;
            
	  delete_queue = del_node->next;
	  del_node->next = NULL;
	  free(del_node); 
	  del_node = NULL;
	}
	else{
	  free(delete_queue);
	  delete_queue = NULL;
	  delete_q_tail = NULL;
	}
	return;
      }
      else{
	delete_la_ll(removeNode->logic_addr, hash_func(removeNode->logic_addr));    
	delete_la2lpn(removeNode, hash_func(delete_queue->logic_addr));
         
	if(delete_queue == delete_q_tail){
	  free(delete_queue);
	  delete_queue = NULL;
	  delete_q_tail = NULL;
	}
	else if(delete_queue->next != NULL){
	  struct delete_node* del_node = delete_queue;
            
	  delete_queue = del_node->next;
	  del_node->next = NULL;
	  free(del_node);
	  del_node = NULL;
	}
      }

      if(delete_queue == NULL) break; 
      
    }

    //background_garbage_collection();      
    //foreground_garbage_collection();
  }
  else
    delete_com = 0;
}


