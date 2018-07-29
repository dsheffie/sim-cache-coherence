#ifndef __cache_controller_hh__
#define __cache_controller_hh__

#include "router.hh"
#include "coherence.hh"
#include <boost/dynamic_bitset.hpp>

class controller {
protected:
  static const int num_lines = 1<<lg2_num_lines;
  typedef router<request_message> request_router_t;
  typedef router<forward_message> forward_router_t;
  typedef router<response_message> response_router_t;
  bool &terminate_simulation;
  int cc_id = -1;
  request_router_t *req_network = nullptr;
  forward_router_t *fwd_network = nullptr;
  response_router_t *rsp_network = nullptr;
  uint8_t cache_lines[num_lines][cl_len];
public:
  controller(bool &terminate_simulation, int cc_id) :
    terminate_simulation(terminate_simulation), cc_id(cc_id) {
    for(int i = 0; i < num_lines; i++) {
      for(int j = 0; j < cl_len; j++) {
	cache_lines[i][j] = 0;
      }
    }
  }
  virtual ~controller() {}
  void hookup_networks(  request_router_t *req_network,
			 forward_router_t *fwd_network,
			 response_router_t *rsp_network) {
    this->req_network = req_network;
    this->fwd_network = fwd_network;
    this->rsp_network = rsp_network;
  }
  void copyLineData(int line_id, uint8_t *d) {
    uint8_t *line = cache_lines[line_id];
    for(int i = 0; i < cl_len; i++) {
      line[i] = d[i];
    }
  }
  virtual void step() = 0;
};

class cache_controller : public controller {
private:
  int directory_id = -1;
  cc_state line_state[num_lines];
public:
  cache_controller(bool &terminate_simulation, int cc_id, int directory_id) :
    controller(terminate_simulation, cc_id), directory_id(directory_id) {
    for(int i = 0; i < num_lines; i++) {
      line_state[i] = cc_state::I;
    }
  }
  cc_state get_line_state(int line) const {
    return line_state[line & (num_lines-1)];
  }
  void step() override;
};

class directory_controller : public controller {
private:
  int n_caches = -1;
  dc_state line_state[num_lines];
  boost::dynamic_bitset<> sharers[num_lines];
  int find_first_shared(int line) const {
    return __builtin_ffs(sharers[line].to_ulong())-1;
  }
public:
  directory_controller(bool &terminate_simulation, int cc_id, int n_caches) :
    controller(terminate_simulation, cc_id), n_caches(n_caches) {
    for(int i = 0; i < num_lines; i++) {
      line_state[i] = dc_state::I;
      sharers[i].resize(n_caches, false);
    }
  }
  dc_state get_line_state(int line) const {
    return line_state[line & (num_lines-1)];
  }
  void step() override;
};




#endif
