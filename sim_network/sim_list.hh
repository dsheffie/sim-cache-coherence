#ifndef __MYLIST_HH__
#define __MYLIST_HH__

#include <iterator>
#include <cstring>
#include <iostream>
#include <type_traits>

template <typename T, typename std::enable_if<std::is_pointer<T>::value,T>::type* = nullptr>
class sim_list {
private:
  class entry {
  private:
    friend class sim_list;
    entry *next,*prev;
    T data;
  public:
    entry(T data = nullptr) : next(nullptr), prev(nullptr), data(data) {}
    const T &getData() const {
      return data;
    }
    T &getData() {
      return data;
    }
    void set_from_head(entry *n) {
      n->next = this;
      prev = n;
    }
    void unlink() {
      if(next) {
	next->prev = prev;
      }
      if(prev) {
	prev->next = next;
      }
    }
    void clear() {
      prev = next = nullptr;
    }
  };
  int64_t num_entries,cnt;
  entry *head, *tail;
  int64_t pool_idx;
  entry **pool, **entries;
  
  entry* alloc(T v) {
    //std::cout << "pool_idx = " << pool_idx << ", cnt = " << cnt << "\n";
    assert(pool_idx >= 0);
    entry * E = pool[pool_idx--];
    //std::cout << "alloc " << E << "\n";
    memset(E, 0, sizeof(entry));
    E->data = v;
    return E;
  }
  void dealloc(entry* e) {
    //std::cout << "pool_idx = " << pool_idx << ", cnt = " << cnt << "\n";
    //std::cout << "dealloc " << e << "\n";
   
    assert(pool_idx < (num_entries-1));
    pool[++pool_idx] = e;
  }
  
public:
  sim_list(size_t num_entries=16) :
    num_entries(num_entries), cnt(0), head(nullptr), tail(nullptr), pool_idx(num_entries-1) {
    pool = new entry*[num_entries];
    entries = new entry*[num_entries];
    for(size_t i = 0; i < num_entries; i++) {
      entries[i] = pool[i] = new entry();
    }
  }
  void resize(size_t num_entries) {
    for(size_t i = 0; i < this->num_entries; i++) {
      delete entries[i];
    }
    delete [] pool;
    delete [] entries;
    head = nullptr;
    tail = nullptr;
    cnt = 0;
    this->num_entries = num_entries;
    pool_idx = num_entries-1;
    pool = new entry*[num_entries];
    entries = new entry*[num_entries];
    for(size_t i = 0; i < num_entries; i++) {
      entries[i] = pool[i] = new entry();
    }
  }
  void clear() {
    head = nullptr;
    tail = nullptr;
    cnt = 0;
    pool_idx = num_entries-1;
    for(size_t i = 0; i < num_entries; i++) {
      pool[i] = entries[i];
      pool[i]->data = nullptr;
    }
  }
  ~sim_list() {
    //std::cout << "num_entries = " << num_entries << "\n" ;
    for(size_t i = 0; i < num_entries; i++) {
      delete entries[i];
    }
    delete [] pool;
    delete [] entries;
  }
  bool full() const {
    return cnt==num_entries;
  }
  T & peek() const {
    //std::cout << "queue list " << this << " : peek (" << cnt << "), entry = " << tail << "\n";
    assert(cnt!=0);
    assert(tail);
    assert(tail->data);
    return tail->data;
  }
  T &peekpeek() {
    if(tail->next)
      return tail->next->data;
    else
      return nullptr;
  }
  /* push to head */
  void push(T v) {
    assert(v != nullptr);
    entry* e = alloc(v);
    //std::cout << "queue list " << " : push (" << cnt << "), entry = " << e << "\n";
    cnt++;
    if(head == nullptr && tail == nullptr) {
      head = tail = e;
    }
    else {
      e->set_from_head(head);
      head = e;
    }
  }
  /* pop from tail */
  T pop() {
    //std::cout << "queue list  : pop (" << cnt << "), entry = " << tail << "\n";
    assert(tail != nullptr);
    entry *ptr = tail;
    T v = ptr->data;
    ptr->unlink();
    tail = ptr->next;
    if(tail==nullptr) {
      head = nullptr;
    }
    dealloc(ptr);
    cnt--;
    return v;
  }
  class const_iterator : public std::iterator<std::forward_iterator_tag, sim_list> {
  protected:
    friend class sim_list;
    entry *ptr;
    ssize_t dist;
    const_iterator(entry *ptr, ssize_t dist) : ptr(ptr), dist(dist) {};
  public:
    const_iterator() : ptr(nullptr), dist(-1) {}
    const bool operator==(const const_iterator &rhs) const {
      return ptr == rhs.ptr;
    }
    const bool operator!=(const const_iterator &rhs) const {
      return ptr != rhs.ptr;
    }
    const T &operator*() const {
      return ptr->getData();
    }
    const_iterator operator++(int postfix) {
      const_iterator it = *this;
      ptr=ptr->next;
      return it;
    }
    const_iterator operator++() {
      ptr=ptr->next;
      return *this;
    }
  };
  
  class iterator : public const_iterator {
  private:
    friend class sim_list;
    iterator(entry *ptr, ssize_t dist) : const_iterator(ptr,dist) {}
  public:
    iterator() : const_iterator() {}
    T &operator*() {
      return iterator::ptr->getData();
    }
  };
  const_iterator begin() const {
    return const_iterator(tail,0);
  }
  const_iterator end() const {
    return const_iterator(nullptr,-1);
  }
  iterator begin() {
    return iterator(tail,0);
  }
  iterator end() {
    return iterator(nullptr,-1);
  }
  void erase(iterator it) {
    if(it == end())
      return;
    it.ptr->unlink();
    if(it.ptr == head) {
      head = it.ptr->prev;
    }
    if(it.ptr == tail) {
      tail = it.ptr->next;
    }
    dealloc(it.ptr);
    cnt--;
  }
  ssize_t distance(iterator it) {
    return it.dist;
  }
  size_t size() const {
    return cnt;
  }
  bool empty() const {
    return cnt==0;
  }

};


#endif
