#include <cassert>
#include "cache_controller.hh"
#include "gthread.hh"
#include "helper.hh"

extern uint64_t clock_cycle;
extern controller **controllers;

static const bool silent = true;

#define print_var(X) {							\
  if(not(silent)) {							\
    std::cout << "actor " << cc_id					\
	      << " : " << #X << " = " << X				\
	      <<" @ cycle " << clock_cycle << "\n";			\
  }									\
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
    X(SM_AD)					\
    X(SM_AD_TO_IM_AD)

#define ENTRY(X) X,
  enum class state {CACHE_STATE_LIST(ENTRY)};
#undef ENTRY

  state curr_state = state::idle;
  state post_fwd_state = state::idle;
  int curr_line  = -1, fwd_line = -1;
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
	  post_fwd_state = state::idle;
	}
	else {
	  curr_line = rand() & 1;
	  if(line_state[curr_line] == cc_state::I)
	    curr_state = state::read;//((rand() % 4 == 0) and line_state[curr_line] != cc_state::I) ? state::write : state::read;
	  else if(line_state[curr_line] == cc_state::S)
	    curr_state = state::write;
	}
	break;
      }
      case state::forward_0: {
	//std::cout << "cache " << cc_id
	//<< " processing forwarded message @ cycle "
	//<< clock_cycle
	//<< "\n";

	switch(fwd_msg.msg_type)
	  {
	  case forward_message_type::Inv: {
	    fwd_line = fwd_msg.addr >> lg2_num_lines;
	    line_state[fwd_line] = cc_state::I;
	    rsp_msg.msg_type = response_message_type::InvAck;
	    rsp_msg.addr = fwd_msg.addr;
	    rsp_msg.fromActor = cc_id;
	    if(rsp_network->send_msg(fwd_msg.reply_to, rsp_msg)) {
	      print_var(rsp_msg.msg_type);
	      print_var(fwd_msg.reply_to);
	      print_var(rsp_msg.addr);
	      curr_state = post_fwd_state;
	    }
	    break;
	  }
	  case forward_message_type::FwdGetS:
	    fwd_line = fwd_msg.addr >> lg2_num_lines;
	    line_state[fwd_line] = cc_state::S;
	    rsp_msg.msg_type = response_message_type::Data;
	    rsp_msg.AckCount = 0;
	    rsp_msg.addr = fwd_msg.addr;
	    rsp_msg.fromActor = cc_id;
	    rsp_msg.setData(cache_lines[fwd_line]);
	    if(rsp_network->send_msg(fwd_msg.reply_to, rsp_msg)) {
	      if(not(silent)) {
		std::cout << cc_id << " needs to reply to "
			  << fwd_msg.reply_to
			  << " with data "
			  << *reinterpret_cast<int*>(rsp_msg.data)
			  << "\n";
	      }
	      curr_state = state::forward_1;
	    }
	    break;
	  case forward_message_type::FwdGetM:
	    fwd_line = fwd_msg.addr >> lg2_num_lines;
	    line_state[fwd_line] = cc_state::I;
	    rsp_msg.msg_type = response_message_type::Data;
	    rsp_msg.addr = fwd_msg.addr;
	    rsp_msg.AckCount = 0;
	    rsp_msg.fromActor = cc_id;
	    rsp_msg.setData(cache_lines[fwd_line]);
	    if(rsp_network->send_msg(fwd_msg.reply_to, rsp_msg)) {
	      curr_state = post_fwd_state;
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
	  curr_state = post_fwd_state;
	}
	break;
      case state::read: {
	auto dc = reinterpret_cast<directory_controller*>(controllers[directory_id]);

	if(not(silent)) {
	  std::cout << "actor " << cc_id
		    << " generated read for line "
		    << curr_line 
		    << ", cache line state = "
		    << line_state[curr_line]
		    << " directory state = "
		    << dc->get_line_state(curr_line)
		    << " @ cycle " << clock_cycle << "\n";
	}
	int n_shared = 0, n_modified = 0;
	for(int i = 0; i < directory_id; i++) {
	  auto cc = reinterpret_cast<cache_controller*>(controllers[i]);
	  if(cc->get_line_state(curr_line)==cc_state::M) {
	    if(not(silent)) {
	      std::cout << "cache " << i << " has line in "
			<< cc->get_line_state(curr_line)
			<< "\n";
	    }
	    n_modified++;
	  }
	}
	if(n_modified > 1) {
	  terminate_simulation = true;
	}
	if(line_state[curr_line] == cc_state::S and n_modified) {
	  terminate_simulation = true;
	}

	request_message msg(request_message_type::GetS, cc_id, curr_line*cl_len);
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

	request_message msg(request_message_type::GetM, cc_id, curr_line*cl_len);
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
	//print_var(*ptr);
	if(not(silent)) {
	  std::cout << "actor " << cc_id << " generated write for line "
		    << curr_line
		    << " , line value = "
		    << *ptr << " @ cycle " << clock_cycle << "\n";
	}
	*ptr = *ptr + 1;
	curr_state = state::idle;
	break;
      }
      case state::IM_AD:
	print_var(inv_needed);
	print_var(inv_recv);

	if(fwd_network->peek_msg(fwd_msg)) {
	  if(not(silent)) {
	    std::cout << "-->cache " << cc_id << " got forwarded message of type "
		      << fwd_msg.msg_type
		      << " in IM_AD @ cycle "
		      << clock_cycle
		      << " for line "
		      << (fwd_msg.addr / cl_len)
		      << " while waiting for line "
		      << curr_line
		      << "\n";
	  }
	}

	
	if(not(rsp_network->recv_msg(rsp_msg)))
	  break;
	switch(rsp_msg.msg_type)
	  {
	  case response_message_type::Data:
	    if(not(silent)) {
	      std::cout << "cache " << cc_id << " GOT DATA FROM DIRECTORY in state "
			<< "state::IM_AD"
			<< " need to wait for "
			<< rsp_msg.AckCount
			<< " sharers\n";
	    }
	    inv_needed = rsp_msg.AckCount;
	    copyLineData(curr_line, rsp_msg.data);
	    break;
	  case response_message_type::InvAck:
	    if(not(silent)) {
	      std::cout << "cache "
			<< cc_id << " got InvAck in state "
			<< "state::IM_AD"
			<< " need to wait for "
			<< inv_needed
			<< " sharers\n";
	      std::cout << ":::::GOT ACK FOR LINE "
			<< (rsp_msg.addr / cl_len)
			<< "\n";
	    }
	    inv_recv++;
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
      case state::SM_AD:
	print_var(inv_needed);
	print_var(inv_recv);

	if(fwd_network->peek_msg(fwd_msg)) {
	  if(not(silent)) {
	    std::cout << "-->cache " << cc_id << " got forwarded message of type "
		      << fwd_msg.msg_type
		      << " in SM_AD @ cycle "
		      << clock_cycle
		      << " for line "
		      << (fwd_msg.addr / cl_len)
		      << " while waiting for line "
		      << curr_line
		      << "\n";
	  }
	  
	  bool line_match = (fwd_msg.addr / cl_len)==curr_line;
	  if(line_match) {
	    if(fwd_msg.msg_type == forward_message_type::Inv) {
	      curr_state = state::SM_AD_TO_IM_AD;
	      fwd_network->pop_msg();
	      break;
	    }
	  }
	  else {
	    post_fwd_state = state::SM_AD;
	    curr_state = state::forward_0;
	    fwd_network->pop_msg();
	    break;
	  }
	}
	if(not(rsp_network->peek_msg(rsp_msg))) {
	  if(not(silent)) {
	    std::cout << "NO RSP MESSAGES FOR " << cc_id << "\n";
	  }
	  break;
	}
	if(not(silent)) {
	  std::cout << "cache " << cc_id
		    << " GOT RSP MESSAGES OF TYPE "
		    << rsp_msg.msg_type
		  << " FOR LINE "
		    << (rsp_msg.addr/cl_len)
		    << " FROM "
		    << rsp_msg.fromActor
		    << "\n";
	}
	
	if((rsp_msg.addr/cl_len) == curr_line) {
	  switch(rsp_msg.msg_type)
	    {
	    case response_message_type::Data:
	      rsp_network->pop_msg();
	      inv_needed = rsp_msg.AckCount;
	      copyLineData(curr_line, rsp_msg.data);
	      break;
	    case response_message_type::InvAck:
	      inv_recv++;
	      if(not(silent)) {
		std::cout << ":::::GOT ACK FOR LINE "
			  << (rsp_msg.addr / cl_len)
			  << " curr line " << curr_line
			  << "\n";
	      }
	      rsp_network->pop_msg();
	      break;
	    default:
	      std::cout << cc_id << " got other message...\n";
	      break;
	    }
	}
	if(inv_needed != -1 ) {
	  print_var(inv_recv);
	  print_var(inv_needed);
	  assert(inv_recv <= inv_needed);
	}
	if(inv_recv == inv_needed) {
	  line_state[curr_line] = cc_state::M;
	  curr_state = state::do_write;
	}
	break;
      case state::SM_AD_TO_IM_AD:
	line_state[fwd_msg.addr >> lg2_num_lines] = cc_state::I;
	rsp_msg.msg_type = response_message_type::InvAck;
	rsp_msg.fromActor = cc_id;
	rsp_msg.addr = fwd_msg.addr;
	inv_recv = 0;
	inv_needed = -1;
	if(rsp_network->send_msg(fwd_msg.reply_to, rsp_msg)) {
	  if(not(silent)) {
	    std::cout << "cache " << cc_id << " acking invalidate to cache "
		      << fwd_msg.reply_to
		      << " for line " << (fwd_msg.addr >> lg2_num_lines)
		      << " at cycle " << clock_cycle
		      << "\n";
	  }
	  curr_state = state::IM_AD;
	}
	break;
      case state::IS_D: {
	if(not(silent)) {
	  std::cout << "CACHE " << cc_id << " WAITING ON " << curr_line << "\n";
	}
	
	if(fwd_network->peek_msg(fwd_msg)) {
	  if(not(silent)) {
	    std::cout << "cc " << cc_id << " "
		      << fwd_msg.msg_type << " to line "
		      << fwd_msg.addr/cl_len
		      << " curr line "
		      << curr_line
		      <<  "\n";
	  }
	  
	  if((fwd_msg.addr/cl_len) != curr_line) {
	    if(not(silent)) {
	      std::cout << "-> " << cc_id
			<< " processing forwarded message "
			<< fwd_msg.msg_type
			<< "\n";
	    }
	    post_fwd_state = state::IS_D;
	    curr_state = state::forward_0;
	    fwd_network->pop_msg();
	    break;
	  }
	}
	if(rsp_network->peek_msg(rsp_msg)) {
	  //std::cout << "CACHE " << cc_id << " GOT_MESSAGE FOR LINE " << (rsp_msg.addr/cl_len)
	  //<< " WAIT FOR " << curr_line
	  //<< "\n";
	  if((rsp_msg.addr / cl_len) == curr_line) {
	    assert(curr_line != -1);
	    line_state[curr_line] = cc_state::S;
	    copyLineData(curr_line, rsp_msg.data);
	    curr_state = state::idle;
	    rsp_network->pop_msg();
	    //std::cout << "CACHE " << cc_id << " POPPED MESSAGE FOR LINE " << (rsp_msg.addr/cl_len) << "\n";
	  }
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
    X(process_GetM_M_SendFwdGetM)		\
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
	curr_line = msg.addr >> lg2_num_lines;
	if(not(silent)) {
	  std::cout << "directory got " << msg.msg_type << " from " << msg.reply_to << " for addr "
		    << std::hex << msg.addr << std::dec
		    << " line " << curr_line
		    << ", line in state " << line_state[curr_line]
		    << " @ cycle " << clock_cycle << "\n";
	}
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
	    switch(line_state[curr_line])
	      {
	      case dc_state::I:
		curr_state = state::process_GetM_I;
		break;
	      case dc_state::S:
		curr_state = state::process_GetM_S_SendData;
		break;
	      case dc_state::M:
		curr_state = state::process_GetM_M_SendFwdGetM;
		break;
	      default:
		die();
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
      case state::process_GetM_M_SendFwdGetM: {
	int owner_id = find_first_shared(curr_line);
	forward_message fwd_msg(forward_message_type::FwdGetM, msg.reply_to, msg.addr);
	if(fwd_network->send_msg(owner_id, fwd_msg)) {
	  curr_state = state::idle;
	  sharers[curr_line][owner_id] = false;
	  sharers[curr_line][msg.reply_to] = true;
	}
	break;
      }
      case state::process_GetS_IS: {
	sharers[curr_line][msg.reply_to] = true;
	line_state[curr_line] = dc_state::S;
	response_message rsp_msg(response_message_type::Data, curr_line*cl_len, 0);
	//std::cout << "DIRECTORY REPLY (GetS_IS) TO " << msg.reply_to << " WITH DATA FOR LINE " << curr_line << " @ cycle " << clock_cycle << "\n";
	rsp_msg.setData(cache_lines[curr_line]);
	rsp_msg.fromActor = cc_id;
	if(rsp_network->send_msg(msg.reply_to, rsp_msg)) {
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
	  //std::cout << "--> DIRECTORY WAITING ON " << owner_id
	  //<< " for line "
	  //<< (msg.addr / cl_len)
	  //<< "\n";
	  curr_state = state::process_GetS_M_WaitForData;
	  sharers[curr_line][msg.reply_to] = true;
	}
	break;
      }
      case state::process_GetS_M_WaitForData:
	assert(line_state[curr_line] == dc_state::S_D);
	if(rsp_network->recv_msg(rsp_msg)) {
	  assert(rsp_msg.msg_type == response_message_type::Data);
	  line_state[curr_line] = dc_state::S;
	  curr_state = state::idle;
	  copyLineData(curr_line, rsp_msg.data);
	}
	break;
      case state::process_GetM_I:
	std::cout << "need to do things for a write to an invalid line\n";
	die();
	break;
      case state::process_GetM_S_SendData: {
	int share_count = sharers[curr_line].count();
	if(sharers[curr_line][msg.reply_to]) {
	  share_count--;
	}
	response_message rsp_msg(response_message_type::Data, curr_line*cl_len,
				 share_count );
	rsp_msg.setData(cache_lines[curr_line]);
	//std::cout << "DIRECTORY REPLY (GetM_S) TO " << msg.reply_to << " WITH DATA FOR LINE " << curr_line << " @ cycle " << clock_cycle << "\n";
	if(rsp_network->send_msg(msg.reply_to, rsp_msg)) {
	  curr_state = state::process_GetM_S_SendInv;
	  sharers[curr_line][msg.reply_to] = true;
	}
	break;
      }
      case state::process_GetM_S_SendInv:
	for(int i = 0; i < sharers[curr_line].size(); i++) {
	  //send invalidation message
	  if(sharers[curr_line][i] and not(i==msg.reply_to)) {
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
