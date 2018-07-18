#include <cassert>
#include "cache_controller.hh"
#include "gthread.hh"

void cache_controller::step() {
  enum class state {idle, forward, read, write, IS_D, IM_AD, SM_AD};
  state curr_state = state::idle;
  forward_message fwd_msg;
  while(not(terminate_simulation)) {
    /* can generate a new request */
    switch(curr_state)
      {
      case state::idle: {
	if(fwd_network->recv_msg(fwd_msg)) {
	  curr_state = state::forward;
	}
	else {
	  curr_state = cc_id==0 and (rand()&1) ? state::write : state::read;
	  curr_line = 0;
	}
	break;
      }
      case state::forward: {
	switch(fwd_msg.msg_type)
	  {
	  case forward_message_type::Inv: {
	    line_state[fwd_msg.addr & (num_lines-1)] = cc_state::I;
	    response_message rsp_msg(response_message_type::InvAck);
	    if(rsp_network->send_msg(fwd_msg.reply_to, rsp_msg)) {
	      std::cout << cc_id << " responded to invalidate message\n";
	      curr_state = state::idle;
	    }
	    break;
	  }
	  default:
	    exit(-1);
	  }
	break;
      }
      case state::read: {
	std::cout << cc_id << " generated read\n";
	request_message msg(request_message_type::GetS, cc_id, 0);
	if(line_state[curr_line] == cc_state::I) {
	  std::cout << cc_id << " : line in invalid\n";
	  if(req_network->send_msg(directory_id, msg)) {
	    curr_state = state::IS_D;
	  }
	}
	else {
	  curr_state = state::idle;
	}
	break;
      }
      case state::write: {
	std::cout << cc_id << " generated write\n";
	request_message msg(request_message_type::GetM, cc_id, 0);
	if(line_state[curr_line] == cc_state::I) {
	  if(req_network->send_msg(directory_id, msg)) {
	    curr_state = state::IM_AD;
	  }
	}
	else if(line_state[curr_line] == cc_state::S) {
	  if(req_network->send_msg(directory_id, msg)) {
	    curr_state = state::SM_AD;
	  }
	}
	else {
	  curr_state = state::idle;
	}
	break;
      }
      case state::IM_AD:
	break;
      case state::SM_AD:
	break;
      case state::IS_D: {
	response_message rsp_msg;
	if(rsp_network->recv_msg(rsp_msg)) {
	  std::cout << "got response to cache controller\n";
	  line_state[curr_line] = cc_state::S;
	  for(int i = 0; i < cl_len; i++) {
	    cache_lines[curr_line][i] = rsp_msg.data[i];
	  }
	  curr_state = state::idle;
	}
	break;
      }
      }
    gthread_yield();
  }
  gthread_terminate();
}


void directory_controller::step() {
  enum class state {idle,process_GetS_IS,process_GetS_M,process_GetM_I,process_GetM_S,process_PutM};
  state curr_state = state::idle;
  int curr_line  = -1;
  request_message msg;
  while(not(terminate_simulation)) {
    switch(curr_state)
      {
      case state::idle: {
	if(not(req_network->recv_msg(msg))) {
	  curr_line = -1;
	  break;
	}
	std::cout << "directory got message from " << msg.reply_to << " for line "
		  << curr_line <<  "\n";
	switch(msg.msg_type)
	  {
	  case request_message_type::GetS:
	    curr_line = msg.addr&(num_lines-1);
	    if(line_state[curr_line]==dc_state::M) {
	      curr_state = state::process_GetS_M;
	    }
	    else {
	      curr_state = state::process_GetS_IS;
	    }
	    break;
	  case request_message_type::GetM:
	    assert(line_state[curr_line]!=dc_state::M);
	    if(line_state[curr_line]==dc_state::I) {
	      curr_state = state::process_GetM_I;
	    }
	    else {
	      curr_state = state::process_GetM_S;
	    }
	    break;
	  case request_message_type::PutM:
	    curr_state = state::process_PutM;
	    break;
	  default:
	    break;
	  }
	break;
      }
      case state::process_GetS_IS: {
	sharers[curr_line][msg.reply_to] = true;
	line_state[curr_line] = dc_state::S;
	response_message rsp_msg(response_message_type::Data, 0);
	rsp_msg.setData(cache_lines[curr_line]);
	if(rsp_network->send_msg(msg.reply_to, rsp_msg)) {
	  std::cout << "directory replying to " << msg.reply_to << "\n";
	  curr_state = state::idle;
	}
	break;
      }
      case state::process_GetS_M:
	std::cout << "need to do things for a write to a shared line\n";
	break;
      case state::process_GetM_I:
	std::cout << "need to do things for a write to an invalid line\n";
	break;
      case state::process_GetM_S:
	std::cout << "need to do things for a write to a shared line\n";
	for(int i = 0; i < sharers[curr_line].size(); i++) {
	  //send invalidation message
	  if(sharers[curr_line][i]) {
	    forward_message fwd_msg(forward_message_type::Inv, msg.reply_to, msg.addr);
	    if(fwd_network->send_msg(i, fwd_msg)) {
	      sharers[curr_line][i] = false;
	    }
	    break;
	  }
	}
	//if(sharers[curr_line].count()==0) {
	//curr_state = state::process_GetM_S_
	//}
	std::cout << "need to invalidate : " << sharers[curr_line].count() << "\n";
	break;
      default:
	break;
      }
    gthread_yield();
  }
  gthread_terminate();
}