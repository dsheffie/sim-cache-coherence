#ifndef __coherencehh__
#define __coherencehh__

#include <map>
#include <iostream>
#include <cstdint>
#include <cstring>

static const int cl_len = 16;
static const int lg2_num_lines = 4;

#define ENUM_LIST_ENTRY(X) X,

#define REQUEST_MESSAGE_LIST(X)			\
  X(GetS)					\
  X(GetM)					\
  X(PutM)					\
  X(Dummy)

enum class request_message_type {REQUEST_MESSAGE_LIST(ENUM_LIST_ENTRY)};

#define FORWARD_MESSAGE_LIST(X)		\
  X(FwdGetS)					\
  X(FwdGetM)					\
  X(Inv)					\
  X(PutAck)					\
  X(Dummy)

enum class forward_message_type {FORWARD_MESSAGE_LIST(ENUM_LIST_ENTRY)};

inline std::ostream & operator<<(std::ostream &out, forward_message_type s) {
#define ENUM_PAIR_ENTRY(X) {forward_message_type::X, #X},
  static const std::map<forward_message_type, std::string> fwd_names = {
    FORWARD_MESSAGE_LIST(ENUM_PAIR_ENTRY)
  };
  out << fwd_names.at(s);
  return out;
#undef ENUM_PAIR_ENTRY
}

#define RESPONSE_MESSAGE_LIST(X)		\
  X(Data)					\
  X(InvAck)					\
  X(Dummy)

enum class response_message_type {RESPONSE_MESSAGE_LIST(ENUM_LIST_ENTRY)};

inline std::ostream & operator<<(std::ostream &out, response_message_type s) {
#define ENUM_PAIR_ENTRY(X) {response_message_type::X, #X},
  static const std::map<response_message_type, std::string> rsp_names = {
    RESPONSE_MESSAGE_LIST(ENUM_PAIR_ENTRY)
  };
  out << rsp_names.at(s);
  return out;
#undef ENUM_PAIR_ENTRY
}

#define CC_STATE_LIST(X)			\
  X(I)						\
  X(IS_D)					\
  X(IM_AD)					\
  X(IM_A)					\
  X(S)						\
  X(SM_AD)					\
  X(SM_A)					\
  X(M)						\
  X(MI_A)					\
  X(SI_A)					\
  X(II_A)

#define DC_STATE_LIST(X)			\
  X(I)						\
  X(S)						\
  X(M)						\
  X(S_D)



enum class cc_state {CC_STATE_LIST(ENUM_LIST_ENTRY)};
enum class dc_state {DC_STATE_LIST(ENUM_LIST_ENTRY)};

inline std::ostream & operator<<(std::ostream &out, cc_state s) {
#define ENUM_PAIR_ENTRY(X) {cc_state::X, #X},
  static const std::map<cc_state, std::string> cc_state_names = {
    CC_STATE_LIST(ENUM_PAIR_ENTRY)
  };
  out << cc_state_names.at(s);
  return out;
#undef ENUM_PAIR_ENTRY
}


inline std::ostream & operator<<(std::ostream &out, dc_state s) {
#define ENUM_PAIR_ENTRY(X) {dc_state::X, #X},
  static const std::map<dc_state, std::string> dc_state_names = {
    DC_STATE_LIST(ENUM_PAIR_ENTRY)
  };
  out << dc_state_names.at(s);
  return out;
#undef ENUM_PAIR_ENTRY
}



struct request_message {
  request_message_type msg_type;
  int reply_to;
  uint32_t addr;
  request_message() :
    msg_type(request_message_type::Dummy), reply_to(-1), addr(0) {}
  request_message(request_message_type msg_type, int reply_to, uint32_t addr=0) :
    msg_type(msg_type),reply_to(reply_to), addr(addr) {}
};

struct forward_message {
  forward_message_type msg_type;
  int reply_to;
  uint32_t addr;
  forward_message() :
    msg_type(forward_message_type::Dummy), reply_to(-1), addr(0) {}
  forward_message(forward_message_type msg_type, int reply_to, uint32_t addr) :
    msg_type(msg_type), reply_to(reply_to), addr(addr) {}
};

struct response_message {
  response_message_type msg_type;
  int AckCount;
  uint8_t data[cl_len] = {0};
  response_message() :
    msg_type(response_message_type::Dummy), AckCount(0) {}
  response_message(response_message_type msg_type, int AckCount = 0) :
    msg_type(msg_type), AckCount(AckCount) {}
  void setData(uint8_t *data_) {
    memcpy(this->data, data_, cl_len);
  }
};



#undef ENUM_LIST_ENTRY

#undef DC_STATE_LIST
#undef CC_STATE_LIST
#undef REQUEST_MESSAGE_LIST
#undef FORWARD_MESSAGE_LIST
#undef RESPONSE_MESSAGE_LIST

#endif
