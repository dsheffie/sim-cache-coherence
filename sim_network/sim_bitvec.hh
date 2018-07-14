#ifndef __sim_bitvec_hh__
#define __sim_bitvec_hh__

#include <cstdlib>
#include <cstring>
#include "helper.hh"

template <typename E>
class sim_bitvec_template {
private:
  static const size_t bpw = 8*sizeof(E);
  static const E all_ones = ~static_cast<E>(0);
  uint64_t n_bits = 0, n_words = 0;
  E *arr = nullptr;
public:
  sim_bitvec_template(uint64_t n_bits = 64) : n_bits(n_bits), n_words((n_bits + bpw - 1) / bpw) {
    arr = new E[n_words];
    memset(arr, 0, sizeof(E)*n_words);
  }
  ~sim_bitvec_template() {
    delete [] arr;
  }
  void clear() {
    memset(arr, 0, sizeof(E)*n_words);
  }
  size_t size() const {
    return static_cast<size_t>(n_bits);
  }
  void clear_and_resize(uint64_t n_bits) {
    delete [] arr;
    this->n_bits = n_bits;
    this->n_words = (n_bits + bpw - 1) / bpw;
    arr = new E[n_words];
    memset(arr, 0, sizeof(E)*n_words);
  }
  bool get_bit(size_t idx) const {
    uint64_t w_idx = idx / bpw;
    uint64_t b_idx = idx % bpw;
    return (arr[w_idx] >> b_idx)&0x1;
  }
  bool operator[](size_t idx) const {
    return get_bit(idx);
  }
  void set_bit(size_t idx) {
    if(idx >= n_bits) {
      die();
    }
    uint64_t w_idx = idx / bpw;
    uint64_t b_idx = idx % bpw;
    arr[w_idx] |= (1UL << b_idx);
  }
  void clear_bit(size_t idx) {
    assert(idx < n_bits);
    uint64_t w_idx = idx / bpw;
    uint64_t b_idx = idx % bpw;
    arr[w_idx] &= ~(1UL << b_idx);
  }
  uint64_t popcount() const {
    uint64_t c = 0;
    if((bpw*n_words) != n_bits) {
      die();
    }
    else {
      for(uint64_t w = 0; w < n_words; w++) {
	c += __builtin_popcountll(arr[w]);
      }
    }
    return c;
  }
  uint64_t num_free() const {
    return n_bits - popcount();
  }
  int64_t find_first_unset() const {
    for(uint64_t w = 0; w < n_words; w++) {
      if(arr[w] == all_ones)
	continue;
      else if(arr[w]==0) {
	return bpw*w;
      }
      else {
	uint64_t x = ~arr[w];
	uint64_t idx = bpw*w + (__builtin_ffsl(x)-1);
	if(idx < n_bits)
	  return idx;
      }
    }
    return -1;    
  }
  int64_t find_first_set() const {
    for(uint64_t w = 0; w < n_words; w++) {
      if(arr[w] == 0)
	continue;
      uint64_t idx = bpw*w + (__builtin_ffsl(arr[w])-1);
      return (idx < n_bits) ? idx : -1;
    }
    return -1;
  }
  
  int64_t find_next_set(int64_t idx) const {
    idx++;
    uint64_t w_idx = idx / bpw;
    uint64_t b_idx = idx % bpw;
    //check current word
    E ww =  (arr[w_idx] >> b_idx) << b_idx;
    if(ww != 0) {
      return w_idx*bpw + (__builtin_ffsl(ww)-1);
    }
    for(uint64_t w = w_idx+1; w < n_words; w++) {
      if(arr[w] == 0)
	continue;
      else {
	uint64_t idx = bpw*w + (__builtin_ffsl(arr[w])-1);
	if(idx < n_bits) {
	  return idx;
	}
	break;
      }
    }
    return -1;    
  }
};

typedef sim_bitvec_template<uint64_t> sim_bitvec;

#endif
