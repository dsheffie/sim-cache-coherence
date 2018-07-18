#ifndef __cachecontrollerhh__
#define __cachecontrollerhh__

#include "router.hh"
#include "coherence.hh"
#include <bitset>

class controller {
protected:
  static const int num_lines = 1<<lg2_num_lines;
  typedef router<request_message> request_router_t;
  typedef router<forward_message> forward_router_t;
  typedef router<response_message> response_router_t;
  const bool &terminate_simulation;
  int cc_id = -1;
  request_router_t *req_network = nullptr;
  forward_router_t *fwd_network = nullptr;
  response_router_t *rsp_network = nullptr;
  
public:
  controller(const bool &terminate_simulation, int cc_id) :
    terminate_simulation(terminate_simulation), cc_id(cc_id) {}
  virtual ~controller() {}
  void hookup_networks(  request_router_t *req_network,
			 forward_router_t *fwd_network,
			 response_router_t *rsp_network) {
    this->req_network = req_network;
    this->fwd_network = fwd_network;
    this->rsp_network = rsp_network;
  }
  virtual void step() = 0;
};

class cache_controller : public controller {
private:
  int directory_id = -1;
  int curr_line  = -1;
  cc_state line_state[num_lines];
  uint8_t cache_lines[num_lines][cl_len];
public:
  cache_controller(const bool &terminate_simulation, int cc_id, int directory_id) :
    controller(terminate_simulation, cc_id), directory_id(directory_id) {
    for(int i = 0; i < num_lines; i++) {
      line_state[i] = cc_state::I;
      for(int j = 0; j < cl_len; j++) {
	cache_lines[i][j] = 0;
      }
    }
  }
  void step() override;
};

class directory_controller : public controller {
private:
  static const int max_caches = 16;
  int n_caches = -1;
  dc_state line_state[num_lines];
  uint8_t cache_lines[num_lines][cl_len];
  std::bitset<max_caches> sharers[num_lines];
public:
  directory_controller(const bool &terminate_simulation, int cc_id, int n_caches) :
    controller(terminate_simulation, cc_id), n_caches(n_caches) {
    for(int i = 0; i < num_lines; i++) {
      line_state[i] = dc_state::I;
      sharers[i].reset();
      for(int j = 0; j < cl_len; j++) {
	cache_lines[i][j] = i;
      }
    }
  }
  void step() override;
};




#endif
