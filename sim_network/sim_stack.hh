#include <cstdint>
#include <cstring>
#include <cassert>
#include "helper.hh"

#ifndef __sim_stack_hh__
#define __sim_stack_hh__

template <typename T>
class sim_stack_template {
protected:
  int64_t stack_sz;
  int64_t idx;
  T *stack;
public:
  sim_stack_template(int64_t stack_sz=32) : stack_sz(stack_sz), idx(stack_sz-1) {
    assert(isPow2(stack_sz));
    stack = new T[stack_sz];
    memset(stack, 0, sizeof(T)*stack_sz);
  }
  void set_tos_idx(int64_t idx) {
    this->idx = idx;
  }
  void copy(const sim_stack_template &other) {
    if(stack) {
      delete [] stack;
    }
    stack = new T[other.stack_sz];
    memcpy(stack, other.stack, sizeof(T)*other.stack_sz);
    stack_sz = other.stack_sz;
    idx = other.idx;
  }
  ~sim_stack_template() {
    delete [] stack;
  }
  void resize(int64_t stack_sz) {
    delete [] stack;
    this->stack_sz = stack_sz;
    idx = stack_sz-1;
    stack = new T[stack_sz];
    memset(stack, 0, sizeof(T)*stack_sz);
  }
  void clear() {
    memset(stack, 0, sizeof(T)*stack_sz);
    idx = stack_sz-1;
  }
  int64_t size() const {
    return idx+1;
  }
  void push(const T &val) {
    stack[idx]=val;
    idx = (idx - 1) & (stack_sz-1);
  }
  T pop() {
    idx = (idx + 1) & (stack_sz - 1);
    return stack[idx];
  }
  int64_t get_tos_idx() const {
    return idx;
  }
};

#endif
