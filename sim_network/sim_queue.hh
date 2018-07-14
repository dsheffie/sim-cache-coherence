#ifndef __sim_queue__
#define __sim_queue__

#include <cstdint>
#include <cassert>


template <typename T>
class sim_queue {
public:
  class funcobj {
  public:
    virtual bool operator()(T e) = 0;

  };
private:
  uint64_t len = 0, len2 = 0;
  T *data = nullptr;
  uint64_t read_idx = 0, write_idx = 0;
  uint64_t push_(T v) {
    uint64_t slot_id = write_idx & (len-1);
    data[slot_id]=v;
    write_idx = (write_idx+1) & (len2-1);
    return slot_id;
  }
  T pop_() {
    T v = data[read_idx & (len-1)];
    read_idx = (read_idx+1) & (len2-1);
    return v;
  }
  const T& peek_() const {
    return data[read_idx & (len-1)];
  }
public:
  sim_queue(uint64_t len = 1) : len(len), len2(2*len) {
    bool pow2 = ((len-1)&len)==0;
    assert(pow2);
    data = new T[len];
    memset(data, 0, sizeof(T)*len);
  }
  void resize(uint64_t len) {
    delete [] data;
    this->len = len;
    len2 = 2*len;
    data = new T[len];
    memset(data, 0, sizeof(T)*len);
    read_idx = write_idx = 0;
  }
  ~sim_queue() {
    delete [] data;
  }
  bool empty() const {
    return read_idx==write_idx;
  }
  bool full() const {
    const uint64_t m_rd_idx = read_idx & (len-1);
    const uint64_t m_wr_idx = write_idx & (len-1);
    return (m_rd_idx==m_wr_idx) && (read_idx != write_idx);
  }
  int64_t traverse_and_apply(funcobj &o) {
    int64_t i = (full() ? write_idx - 1 : write_idx) & (len-1);
    int64_t d = read_idx & (len-1);
    int64_t c = 0;
    while(true) {
      c += o(data[i]);
      data[i] = nullptr;
      if(i==d) {
	break;
      }
      i--;
      i &= (len-1);
    }
    return c;
  }
  void clear() {
    memset(data, 0, sizeof(T)*len);
    read_idx = write_idx = 0;
  }
  uint64_t push(T v) {
    assert(!full());
    return push_(v);
  }
  T pop() {
    assert(!empty());
    return pop_();
  }
  const T & peek() const {
    assert(!empty());
    return peek_();
  }
  T peek_next_pop() const {
    uint64_t next_rd = (read_idx+1) & (len2-1);
    return data[next_rd & (len-1)];
  }
  size_t size() const {
    return len;
  }
  uint64_t get_read_idx() const {
    return read_idx & (len-1);
  }
  uint64_t get_write_idx() const {
    return write_idx & (len-1);
  }
  T &at(size_t idx) {
    return data[idx];
  }
};


#endif
