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
#include "coherence.hh"
#include "cache_controller.hh"

typedef router<request_message> request_router_t;
typedef router<forward_message> forward_router_t;
typedef router<response_message> response_router_t;

static bool terminate_simulation = false;
uint64_t clock_cycle = 0;
static uint64_t max_cycles = 100;
controller **controllers = nullptr;

static int sent_messages = -1, streak = 0;

static const uint32_t n_caches = 2;
static const uint32_t n_routers = n_caches+1;

template<typename RT>
void step_router(void *arg) {
  RT *router = reinterpret_cast<RT*>(arg);
  int r_id = router->get_id();
  while(not(terminate_simulation)) {
    sent_messages += router->tick();
    gthread_yield();
  }
  std::cout << "(termination) router " << r_id << " @ " << clock_cycle << "\n";
  gthread_terminate();
}

extern "C" {
  void step_clock(void *arg) {
    while(not(terminate_simulation)) {
      clock_cycle++;
      if(sent_messages == 0) {
	streak++;
      }
      else {
	streak = 0;
      }
      if(streak > (8*n_routers)) {
	std::cout << "streak of " << streak << " cycles without progres\n";
	terminate_simulation = true;
      }
      if(clock_cycle > max_cycles) {
	terminate_simulation = true;
      }
      sent_messages = 0;
      gthread_yield();
    }
    terminate_simulation = true;
    gthread_terminate();
  }
  void step_cc(void *arg) {
    cache_controller *cc = reinterpret_cast<cache_controller*>(arg);
    cc->step();
  }
  void step_dc(void *arg) {
    directory_controller *dc = reinterpret_cast<directory_controller*>(arg);
    dc->step();
  }

  void step_request_router(void *arg) {
    step_router<request_router_t>(arg);
  }
  void step_forward_router(void *arg) {
    step_router<forward_router_t>(arg);
  }
  void step_response_router(void *arg) {
    step_router<response_router_t>(arg);
  }

  
};

int main(int argc, char *argv[]) {
  srand(3);
  if(argc > 1) {
    max_cycles = atoi(argv[1]);
  }
  request_router_t **req_routers = new request_router_t*[n_routers];
  forward_router_t **fwd_routers = new forward_router_t*[n_routers];
  response_router_t **rsp_routers = new response_router_t*[n_routers];
  
  controllers = new controller*[n_routers];
  for(uint32_t i = 0; i < (n_routers-1); i++) {
    controllers[i] = new cache_controller(terminate_simulation, i, n_routers-1);
  }
  controllers[n_routers-1] = new directory_controller(terminate_simulation,
						      n_routers-1,
						      n_routers-1);
  
  for(uint32_t i = 0; i < n_routers; i++) {
    req_routers[i] = new request_router_t(req_routers, i, n_routers);
    fwd_routers[i] = new forward_router_t(fwd_routers, i, n_routers);
    rsp_routers[i] = new response_router_t(rsp_routers, i, n_routers);
  }
  
  for(uint32_t i = 0; i < n_routers; i++) {
    req_routers[i]->hookup_ring(req_routers[(i-1) % n_routers], req_routers[(i+1) % n_routers]);
    fwd_routers[i]->hookup_ring(fwd_routers[(i-1) % n_routers], fwd_routers[(i+1) % n_routers]);
    rsp_routers[i]->hookup_ring(rsp_routers[(i-1) % n_routers], rsp_routers[(i+1) % n_routers]);
  }

  for(uint32_t i = 0; i < n_routers; i++) {
    controllers[i]->hookup_networks(req_routers[i], fwd_routers[i], rsp_routers[i]);
  }

  gthread::make_gthread(&step_clock, nullptr);
  for(uint32_t i = 0; i < n_routers; i++) {
    gthread::make_gthread(&step_request_router, reinterpret_cast<void*>(req_routers[i]));
    gthread::make_gthread(&step_forward_router, reinterpret_cast<void*>(fwd_routers[i]));
    gthread::make_gthread(&step_response_router, reinterpret_cast<void*>(rsp_routers[i]));
  }
  for(uint32_t i = 0; i < (n_routers-1); i++) {
    gthread::make_gthread(&step_cc, reinterpret_cast<void*>(controllers[i]));
  }
  gthread::make_gthread(&step_dc, reinterpret_cast<void*>(controllers[n_routers-1]));
  start_gthreads();

  for(uint32_t i = 0; i < n_routers; i++) {
    delete controllers[i];
  }
  
  for(uint32_t i = 0; i < n_routers; i++) {
    delete req_routers[i];
    delete fwd_routers[i];
    delete rsp_routers[i];
  }

  delete [] controllers;
  delete [] req_routers;
  delete [] fwd_routers;
  delete [] rsp_routers;

  gthread::free_threads();
  
  return 0;
}

