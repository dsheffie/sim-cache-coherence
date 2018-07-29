#ifndef __routerhh__
#define __routerhh__

#include <iostream>

#include "sim_queue.hh"

template <typename M>
class router {
private:
  static const int n_ports = 2;
  router **routers;
  int router_id, n_routers,port_priority,credits;

  
  struct MM {
    int src;
    int dst;
    int hops;
    bool route_left;
    M msg;
    MM() {}
    MM(int src, int dst, M msg) :
      src(src), dst(dst), hops(0), route_left(false), msg(msg) {}
    friend std::ostream &operator<<(std::ostream &out, const MM &mm) {
      out << "(" << "src=" << mm.src
	  << ",dst=" << mm.dst
	  << ",msg=" << mm.msg << ")";
      return out;
    }
  };
  sim_queue<MM> *input_buffers[n_ports+1];
  router *output_ports[n_ports];
  sim_queue<MM> *cpu_input, *cpu_output;

  bool accept_from_right_port(const MM &msg_) {
    if(not(output_ports[1]->input_buffers[0]->full())) {
      auto msg  = msg_; msg.hops++;
      output_ports[1]->input_buffers[0]->push(msg);
      return true;
    }
    return false;
  }

  bool accept_from_left_port(const MM &msg_) {
    if(not(output_ports[0]->input_buffers[1]->full())) {
      auto msg  = msg_; msg.hops++;
      output_ports[0]->input_buffers[1]->push(msg);
      return true;
    }
    return false;
  }
  
  bool route_msg(const MM &msg_) {
#if 0
    std::cout << "router " << router_id << " : out"
	      << output_ports[0]->get_id() << " "
	      << output_ports[0]->input_buffers[1]->full()
	      << ", out"
      	      << output_ports[1]->get_id() << " "
	      << output_ports[1]->input_buffers[0]->full()
	      << ", cpu_output "
	      << cpu_output->full()
	      << ", cpu_input "
	      << cpu_input->full()
	      << "\n";
#endif
    
    if(msg_.dst == router_id) {
      if(not(cpu_output->full())) {
	auto msg  = msg_; msg.hops++;
	cpu_output->push(msg);
	routers[msg.src]->credits++;
	return true;
      }
      else {
	std::cout << "router " << router_id << " failed to push to cpu output queue\n";
      }
    }
    else if(not(msg_.route_left)) {
      if(accept_from_right_port(msg_)) {
	return true;
      }
    }
    else {
      if(accept_from_left_port(msg_)) {
	return true;
      }
    }

    return false;
  }
public:
  router(router **routers, int router_id, int n_routers, int buflen = 64) :
    routers(routers), router_id(router_id),
    n_routers(n_routers), port_priority(0), credits(buflen/2) {
    for(int i = 0; i < n_ports; i++) {
      input_buffers[i] = new sim_queue<MM>(buflen);
      output_ports[i] = nullptr;
    }
    cpu_input = new sim_queue<MM>(buflen);
    cpu_output = new sim_queue<MM>(buflen);
    input_buffers[n_ports] = cpu_input;
  }
  ~router() {
    for(int i = 0; i < n_ports; i++) {
      delete input_buffers[i];
    }
    delete cpu_input;
    delete cpu_output;
  }
  int get_id() const {
    return router_id;
  }
  void hookup_ring(router *prev, router *next) {
    output_ports[0] = prev;
    output_ports[1] = next;
  }
  bool send_msg(int dst, const M &msg) {
    if(dst != router_id) {
      if(credits >0 and not(cpu_input->full())) {
	MM msg_(router_id, dst, msg);
	cpu_input->push(msg_);
	credits--;
	return true;
      }
    }
    else {
      if(not(cpu_output->full())) {
	  MM msg_(router_id, dst, msg);
	  int left_dist = router_id - msg_.dst;
	  int right_dist = router_id - (msg_.dst + n_routers);

	  if(left_dist < 0) {
	    left_dist = -left_dist;
	  }
	  if(right_dist < 0) {
	    right_dist = -right_dist;
	  }
	  msg_.route_left = left_dist < right_dist;
	  
	  cpu_output->push(msg_);
	  return true;
      }
    }
    //std::cout << "router " << router_id << " couldn't send\n";
    return false;
  }

  bool recv_msg(M &msg) {
    if(not(cpu_output->empty())) {
      MM msg_ = cpu_output->pop();
      msg = msg_.msg;
      return true;
    }
    return false;
  }
  bool peek_msg(M &msg) {
    if(not(cpu_output->empty())) {
      MM msg_ = cpu_output->peek();
      msg = msg_.msg;
      return true;
    }
    return false;
  }
  void pop_msg() {
    assert(not(cpu_output->empty()));
    cpu_output->pop();
  }
  int tick() {
    int msg_cnt = 0;
    for(int ii = 0; ii < (n_ports+1); ii++) {
      int i = (ii + port_priority) % (n_ports+1);
      if(not(input_buffers[i]->empty())) {
	if(route_msg(input_buffers[i]->peek())) {
	  input_buffers[i]->pop();
	  msg_cnt++;
	  //std::cout << "router " << router_id << " routed message from port " << i << "\n";
	}
      }
    }
    port_priority = (1+port_priority) % (n_ports+1);
    return msg_cnt;
  }
};

#endif
