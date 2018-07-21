#include <cassert>
#include "cache_controller.hh"
#include "gthread.hh"
#include "helper.hh"

extern uint64_t clock_cycle;
extern controller **controllers;

#define print_var(X) {							\
    std::cout << "actor " << cc_id					\
	      << " : " << #X << " = " << X				\
	      <<" @ cycle " << clock_cycle << "\n";			\
  }

void cache_controller::step() {
#define CACHE_STATE_LIST(X)			\
  X(idle)					\
    X(forward_0)				\
    X(forward_1)				\
    X(read)					\
    X(write)					\
    X(do_write)					\
    X(IS_D)					\
    X(IM_AD)					\
    X(SM_AD)

#define ENTRY(X) X,
  enum class state {CACHE_STATE_LIST(ENTRY)};
#undef ENTRY

  state curr_state = state::idle;
  forward_message fwd_msg;
  response_message rsp_msg;
  int inv_recv = 0, inv_needed = -1;
  while(not(terminate_simulation)) {
    /* can generate a new request */
    switch(curr_state)
      {
      case state::idle: {
	if(fwd_network->recv_msg(fwd_msg)) {
	  curr_state = state::forward_0;
	}
	else {
	  curr_state = cc_id==0 and (rand()&1) ? state::write : state::read;
	  curr_line = 0;//rand() & (num_lines-1);
	}
	break;
      }
      case state::forward_0: {
	switch(fwd_msg.msg_type)
	  {
	  case forward_message_type::Inv: {
	    line_state[fwd_msg.addr & (num_lines-1)] = cc_state::I;
	    rsp_msg.msg_type = response_message_type::InvAck;
	    rsp_msg.fromActor = cc_id;
	    if(rsp_network->send_msg(fwd_msg.reply_to, rsp_msg)) {
	      print_var(rsp_msg.msg_type);
	      curr_state = state::idle;
	    }
	    break;
	  }
	  case forward_message_type::FwdGetS:
	    curr_line = fwd_msg.addr & (num_lines-1);
	    line_state[curr_line] = cc_state::S;
	    rsp_msg.msg_type = response_message_type::Data;
	    rsp_msg.AckCount = 0;
	    rsp_msg.fromActor = cc_id;
	    rsp_msg.setData(cache_lines[curr_line]);
	    if(rsp_network->send_msg(fwd_msg.reply_to, rsp_msg)) {
	      std::cout << cc_id << " needs to reply to "
			<< fwd_msg.reply_to << "\n";
	      curr_state = state::forward_1;
	    }
	    break;
	  default:
	    std::cout << "cache " << cc_id << " wtf : " << fwd_msg.msg_type << "\n";
	    die();
	  }
	break;
      }
      case state::forward_1:
	if(rsp_network->send_msg(directory_id, rsp_msg)) {
	  curr_state = state::idle;
	}
	break;
      case state::read: {
	auto dc = reinterpret_cast<directory_controller*>(controllers[directory_id]);
#if 0
	std::cout << "actor " << cc_id
		  << " generated read, cache line state = "
		  << line_state[curr_line]
		  << " directory state = "
		  << dc->get_line_state(curr_line)
		  << " @ cycle " << clock_cycle << "\n";
#endif
	int n_shared = 0, n_modified = 0;
	for(int i = 0; i < directory_id; i++) {
	  auto cc = reinterpret_cast<cache_controller*>(controllers[i]);
	  if(cc->get_line_state(curr_line)==cc_state::M) {
	    n_modified++;
	  }
	}
	assert(n_modified <= 1);
	if(line_state[curr_line] == cc_state::S and n_modified) {
	  die();
	}
	// if(dc->get_line_state(curr_line) == dc_state::M and
	//    line_state[curr_line] == cc_state::S) {
	//   die();
	// }
	request_message msg(request_message_type::GetS, cc_id, 0);
	if(line_state[curr_line] == cc_state::I) {
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
	inv_recv = 0;
	inv_needed = -1;
	std::cout << "actor " << cc_id << " generated write, line state = "
		  << line_state[curr_line] << " @ cycle " << clock_cycle << "\n";
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
	  assert(line_state[curr_line] == cc_state::M);
	  curr_state = state::do_write;
	}
	break;
      }
      case state::do_write: {
	int *ptr = reinterpret_cast<int*>(&cache_lines[curr_line]);
	ptr++;
	curr_state = state::idle;
	break;
      }
      case state::IM_AD:
	if(not(rsp_network->peek_msg(rsp_msg)))
	  break;
	switch(rsp_msg.msg_type)
	  {
	  case response_message_type::Data:
	    std::cout << "GOT DATA FROM DIRECTORY, need to wait for "
		      << rsp_msg.AckCount << " sharers\n";
	    rsp_network->pop_msg();
	    inv_needed = rsp_msg.AckCount;
	    break;
	  default:
	    std::cout << cc_id << " got other message...\n";
	    break;
	  }
	break;
      case state::SM_AD:
	print_var(inv_needed);
	print_var(inv_recv);
	if(not(rsp_network->peek_msg(rsp_msg)))
	  break;
	switch(rsp_msg.msg_type)
	  {
	  case response_message_type::Data:
	    rsp_network->pop_msg();
	    inv_needed = rsp_msg.AckCount;
	    for(int i = 0; i < cl_len; i++) {
	      cache_lines[curr_line][i] = rsp_msg.data[i];
	    }
	    break;
	  case response_message_type::InvAck:
	    inv_recv++;
	    rsp_network->pop_msg();
	    break;
	  default:
	    std::cout << cc_id << " got other message...\n";
	    break;
	  }
	if(inv_recv == inv_needed) {
	  line_state[curr_line] = cc_state::M;
	  curr_state = state::do_write;
	}
	break;

      case state::IS_D: {
	if(rsp_network->recv_msg(rsp_msg)) {
	  std::cout << "CACHE " << cc_id << " got response from cache controller @ cycle "
		    << clock_cycle << "\n";
	  assert(curr_line != -1);
	  line_state[curr_line] = cc_state::S;
	  std::cout << "cache " << cc_id << " line " << curr_line
		    << " now in state " << line_state[curr_line] << "\n";
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
  
#define CASE_STMT(X) {							\
    case state::X:							\
      std::cout << "actor " << cc_id << " in state " << #X << " state\n"; \
      break;								\
  }
  switch(curr_state)
    {
      CACHE_STATE_LIST(CASE_STMT)
    }
#undef CACHE_STATE_LIST
#undef CASE_STMT  
  gthread_terminate();
}


void directory_controller::step() {
#define DIRECTORY_STATE_LIST(X)			\
  X(idle)					\
    X(process_GetS_IS)				\
    X(process_GetS_M_SendFwdGetS)		\
    X(process_GetS_M_WaitForData)		\
    X(process_GetM_I)				\
    X(process_GetM_S_SendInv)			\
    X(process_GetM_S_SendData)			\
    X(process_PutM)
  
#define ENTRY(X) X,
  enum class state {DIRECTORY_STATE_LIST(ENTRY)};

#undef ENTRY
  state curr_state = state::idle;
  int curr_line  = -1;
  request_message msg;
  response_message rsp_msg;
  while(not(terminate_simulation)) {
    switch(curr_state)
      {
      case state::idle: {
	if(not(req_network->recv_msg(msg))) {
	  curr_line = -1;
	  break;
	}
	curr_line = msg.addr&(num_lines-1);
	std::cout << "directory got " << msg.msg_type << " from " << msg.reply_to << " for line "
		  << curr_line
		  << ", line in state " << line_state[curr_line]
		  << " @ cycle " << clock_cycle << "\n";
	switch(msg.msg_type)
	  {
	  case request_message_type::GetS:
	    if(line_state[curr_line]==dc_state::M) {
	      line_state[curr_line] = dc_state::S_D;
	      curr_state = state::process_GetS_M_SendFwdGetS;
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
	      curr_state = state::process_GetM_S_SendData;
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
	rsp_msg.fromActor = cc_id;
	if(rsp_network->send_msg(msg.reply_to, rsp_msg)) {
	  std::cout << "directory replying to " << msg.reply_to << "\n";
	  curr_state = state::idle;
	}
	break;
      }
      case state::process_GetS_M_SendFwdGetS: {
	int owner_id = find_first_shared(curr_line);
	print_var(owner_id);
	assert(owner_id >= 0);
	forward_message fwd_msg(forward_message_type::FwdGetS, msg.reply_to, msg.addr);
	if(fwd_network->send_msg(owner_id, fwd_msg)) {
	  curr_state = state::process_GetS_M_WaitForData;
	  sharers[curr_line][msg.reply_to] = true;
	}
	break;
      }
      case state::process_GetS_M_WaitForData:
	assert(line_state[curr_line] == dc_state::S_D);
	if(rsp_network->recv_msg(rsp_msg)) {
	  std::cout << "GOT REPLY FROM LINE IN M STATE @ cycle " << clock_cycle << "\n";
	  line_state[curr_line] = dc_state::S;
	  curr_state = state::idle;
	}
	break;
      case state::process_GetM_I:
	std::cout << "need to do things for a write to an invalid line\n";
	break;
      case state::process_GetM_S_SendData: {
	std::cout << "DIRECTORY : need to do things for a write to a shared line, sharer count = "
		  << sharers[curr_line].count() << "\n";
	int share_count = sharers[curr_line].count();
	if(sharers[curr_line][msg.reply_to]) {
	  share_count--;
	}
	response_message rsp_msg(response_message_type::Data, share_count );
	rsp_msg.setData(cache_lines[curr_line]);
	if(rsp_network->send_msg(msg.reply_to, rsp_msg)) {
	  curr_state = state::process_GetM_S_SendInv;
	  sharers[curr_line][msg.reply_to] = true;
	}
	else {
	  std::cout << "directory can't send rsp message in state process_GetM_S_SendData\n";
	}
	break;
      }
      case state::process_GetM_S_SendInv:
	for(int i = 0; i < sharers[curr_line].size(); i++) {
	  //send invalidation message
	  if(sharers[curr_line][i] and not(i==msg.reply_to)) {
	    std::cout << "DIRECTORY INVALIDATING " << i << "\n";
	    forward_message fwd_msg(forward_message_type::Inv, msg.reply_to, msg.addr);
	    if(fwd_network->send_msg(i, fwd_msg)) {
	      sharers[curr_line][i] = false;
	    }
	    break;
	  }
	}
	if(sharers[curr_line].count()==1) {
	  curr_state = state::idle;
	  line_state[curr_line] = dc_state::M;
	  //curr_state = state::process_GetM_S_SendData;
	}
	break;

      default:
	break;
      }
    gthread_yield();
  }
#define CASE_STMT(X) {							\
    case state::X:							\
      std::cout << "actor " << cc_id << " in state " << #X << " state\n"; \
      break;								\
  }
  switch(curr_state)
    {
      DIRECTORY_STATE_LIST(CASE_STMT)
    }
#undef DIRECTORY_STATE_LIST
#undef CASE_STMT  
  gthread_terminate();
}
