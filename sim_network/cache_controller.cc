#include <cassert>
#include "cache_controller.hh"
#include "gthread.hh"
#include "helper.hh"


void cache_controller::step() {
  enum class state {idle, forward, read, write, IS_D, IM_AD, SM_AD};
  state curr_state = state::idle;
  forward_message fwd_msg;
  response_message inv_ack_msg(response_message_type::InvAck);
  response_message rsp_msg;
  int inv_recv = 0, inv_needed = -1;
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
	  curr_line = 0;//rand() & (num_lines-1);
	}
	break;
      }
      case state::forward: {
	switch(fwd_msg.msg_type)
	  {
	  case forward_message_type::Inv: {
	    line_state[fwd_msg.addr & (num_lines-1)] = cc_state::I;
	    if(rsp_network->send_msg(fwd_msg.reply_to, inv_ack_msg)) {
	      std::cout << cc_id << " responded to invalidate message\n";
	      curr_state = state::idle;
	    }
	    break;
	  }
	  default:
	    std::cout << "cache " << cc_id << " wtf : " << fwd_msg.msg_type << "\n";
	    die();
	  }
	break;
      }
      case state::read: {
	std::cout << cc_id << " generated read, line state = "
		  << line_state[curr_line] << "\n";
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
	std::cout << cc_id << " generated write, line state = "
		  << line_state[curr_line] << "\n";
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
	  curr_state = state::idle;
	}
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
	if(not(rsp_network->peek_msg(rsp_msg)))
	  break;
	switch(rsp_msg.msg_type)
	  {
	  case response_message_type::Data:
	    std::cout << "GOT DATA FROM DIRECTORY, need to wait for "
		      << rsp_msg.AckCount << " sharers\n";
	    rsp_network->pop_msg();
	    inv_needed = rsp_msg.AckCount;
	    for(int i = 0; i < cl_len; i++) {
	      cache_lines[curr_line][i] = rsp_msg.data[i];
	    }
	    break;
	  case response_message_type::InvAck:
	    inv_recv++;
	    rsp_network->pop_msg();
	    if(inv_recv == inv_needed) {
	      std::cout << "GOT INVALIDATIONS!\n";
	      line_state[curr_line] = cc_state::M;
	      curr_state = state::idle;
	    }
	    break;
	  default:
	    std::cout << cc_id << " got other message...\n";
	    break;
	  }
	break;

      case state::IS_D: {
	if(rsp_network->recv_msg(rsp_msg)) {
	  std::cout << "CACHE " << cc_id << " got response from cache controller\n";
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

  switch(curr_state)
    {
    case state::idle:
      std::cout << cc_id << " in idle state\n";
      break;
    case state::forward:
      std::cout << cc_id << " in forward state\n";
      break;
    case state::read:
      std::cout << cc_id << " in read state\n";
      break;
    case state::write:
      std::cout << cc_id << " in write state\n";
      break;
    case state::IS_D:
      std::cout << cc_id << " in IS_D state\n";
      break;
    case state::IM_AD:
      std::cout << cc_id << " in IM_AD state\n";
      break;
    case state::SM_AD:
      std::cout << cc_id << " in SM_AD state\n";
      break;
    default:
      std::cout << cc_id << " in unknown state\n";
      break;
    }
  gthread_terminate();
}


void directory_controller::step() {
  enum class state {idle,
		    process_GetS_IS,
		    process_GetS_M_SendFwdGetS,
		    process_GetS_M_WaitForData,
		    process_GetM_I,
		    process_GetM_S_SendInv,
		    process_GetM_S_SendData,
		    process_PutM};
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
	curr_line = msg.addr&(num_lines-1);
	std::cout << "directory got message from " << msg.reply_to << " for line "
		  << curr_line <<  "\n";
	switch(msg.msg_type)
	  {
	  case request_message_type::GetS:
	    if(line_state[curr_line]==dc_state::M) {
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
	if(rsp_network->send_msg(msg.reply_to, rsp_msg)) {
	  std::cout << "directory replying to " << msg.reply_to << "\n";
	  curr_state = state::idle;
	}
	break;
      }
      case state::process_GetS_M_SendFwdGetS: {
	int owner_id = find_first_shared(curr_line);
	std::cout << "need to do things for a read to a modified line, owner "
		  <<  owner_id << "\n";
	assert(owner_id >= 0);
	forward_message fwd_msg(forward_message_type::FwdGetS, msg.reply_to, msg.addr);
	if(fwd_network->send_msg(owner_id, fwd_msg)) {
	  std::cout << "sent fwdGetS to " << owner_id << "\n";
	  curr_state = state::process_GetS_M_WaitForData;
	}
	break;
      }
      case state::process_GetS_M_WaitForData:
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
	  line_state[curr_line] = dc_state::M;
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
	  //curr_state = state::process_GetM_S_SendData;
	}
	break;

      default:
	break;
      }
    gthread_yield();
  }
  gthread_terminate();
}
