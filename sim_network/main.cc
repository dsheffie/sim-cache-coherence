#include <cstdio>
#include <iostream>
#include <set>
#include <fstream>
#include <map>

#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include <cxxabi.h>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cassert>

#include "gthread.hh"
#include "router.hh"

typedef router<int> ROUTER_T;
static const uint32_t n_routers = 32;

bool terminate_simulation = false;

int sent_messages = -1;
static uint64_t clock_cycle = 0;
extern "C" {
  void step_clock(void *arg) {
    while(clock_cycle < (1<<25)) {
      clock_cycle++;
      //if(sent_messages == 0) {
      //terminate_simulation = true;
      // }
      sent_messages = 0;
      gthread_yield();
    }
    terminate_simulation = true;
    gthread_terminate();
  }
  
  void step_router(void *arg) {
   ROUTER_T *router = reinterpret_cast<ROUTER_T*>(arg);
   int r_id = router->get_id();
   uint64_t n_msgs = 0;
   while(not(terminate_simulation)) {
     router->tick();
     
     if(n_msgs < (1<<20)) {
       int id = 0;
       do {
	 id = rand()%n_routers;
       } while(id==r_id);
       
       if(router->send_msg(id, -1)) {
	 n_msgs++;
	 sent_messages++;
       }
       //std::cout << "router " << r_id << ": n_msgs = " << n_msgs << "\n";
     }

     gthread_yield();
   }
   std::cout << "(termination) router " << r_id << ": n_msgs = " << n_msgs << "\n";
   gthread_terminate();
  }
};


int main(int argc, char *argv[]) {
  ROUTER_T **routers = new ROUTER_T*[n_routers];
  for(uint32_t i = 0; i < n_routers; i++) {
    routers[i] = new ROUTER_T(routers, i, n_routers);
  }
  for(uint32_t i = 0; i < n_routers; i++) {
    routers[i]->hookup(routers[(i-1) % n_routers], routers[(i+1) % n_routers]);
  }

  routers[0]->send_msg(3, -1);

  gthread::make_gthread(&step_clock, nullptr);
  for(uint32_t i = 0; i < n_routers; i++) {
    gthread::make_gthread(&step_router, reinterpret_cast<void*>(routers[i]));
  }

  start_gthreads();
  
  for(uint32_t i = 0; i < n_routers; i++) {
    delete routers[i];
  }
  delete [] routers;


  
  return 0;
}

