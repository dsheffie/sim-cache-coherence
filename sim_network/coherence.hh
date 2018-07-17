#ifndef __coherencehh__
#define __coherencehh__

#include <unordered_map>
#include <cstdint>
#include <cstring>

static const int cl_len = 16;

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

#define RESPONSE_MESSAGE_LIST(X)		\
  X(Data)					\
  X(InvAck)					\
  X(Dummy)

enum class response_message_type {RESPONSE_MESSAGE_LIST(ENUM_LIST_ENTRY)};

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

#define DS_STATE_LIST(X)			\
  X(I)						\
  X(S)						\
  X(M)						\
  X(S_D)



enum class cc_state {CC_STATE_LIST(ENUM_LIST_ENTRY)};
enum class ds_state {DS_STATE_LIST(ENUM_LIST_ENTRY)};

struct request_message {
  request_message_type msg_type;
  uint32_t addr;
  request_message() :
    msg_type(request_message_type::Dummy), addr(0) {}
  request_message(request_message_type msg_type, uint32_t addr=0) :
    msg_type(msg_type),addr(addr) {}
};

struct forward_message {
  forward_message_type msg_type;
  int reply_to;
  forward_message() :
    msg_type(forward_message_type::Dummy), reply_to(-1) {}
  forward_message(forward_message_type msg_type, int reply_to) :
    msg_type(msg_type), reply_to(reply_to) {}
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

#undef DS_STATE_LIST
#undef CC_STATE_LIST
#undef REQUEST_MESSAGE_LIST
#undef FORWARD_MESSAGE_LIST
#undef RESPONSE_MESSAGE_LIST

#endif
