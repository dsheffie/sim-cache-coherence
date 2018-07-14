#ifndef __gthreadhh__
#define __gthreadhh__

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <list>

void start_gthreads();
void gthread_yield();
void gthread_terminate();




class gthread {
 public:
  typedef gthread* gthread_ptr;
private:
#ifdef __aarch64__
  static const int num_saved_regs = 13;
#endif
#ifdef __amd64__
  static const int num_saved_regs = 7;
#endif
  typedef void (*callback_t)(void*);
  static const size_t stack_sz = 1<<20;
  enum class thread_status {uninitialized,ready,run};
  static gthread_ptr head;
  static std::list<gthread_ptr> threads;
  static int64_t uuidcnt;
  int64_t id = -1;
  callback_t fptr = nullptr;
  void *arg = nullptr;
  uint8_t *stack_ptr = nullptr;
  thread_status status = thread_status::uninitialized;
  gthread_ptr next = nullptr;
  gthread_ptr prev = nullptr;
  uint64_t state[num_saved_regs] = {0};
  uint8_t stack_alloc[stack_sz] __attribute__((aligned(16))) = {0};
  int64_t get_id() const {
    return id;
  }
  void remove_from_list() {
    if(next) {
      next->prev = prev;
    }
    if(prev) {
      prev->next = next;
    }
    if(head == this) {
      head = next;
    }
  }
  static bool valid_head() {
    return (head != nullptr);
  }
  void insert_into_list() {
    if(head==nullptr) {
      head = this;
    }
    else {
      gthread_ptr ptr = head;
      while(ptr->next != nullptr) {
	ptr = ptr->next;
      }
      ptr->next = this;
      prev = ptr;
    }
  }
  gthread_ptr get_next() const {
    if(next==nullptr)
      return head;
    else
      return next;
  }
  gthread(callback_t fptr, void *arg) : id(uuidcnt++), fptr(fptr),
					arg(arg), stack_ptr(stack_alloc + stack_sz - 16) {}
public:
  static void make_gthread(callback_t fptr, void *arg) {
    /* delegate ctor to helper class */
    auto t = new gthread(fptr, arg);
    gthread::threads.push_back(t);
    t->insert_into_list();
  }
  static void free_threads() {
    for(gthread_ptr thr : gthread::threads) {
      delete thr;
    }
    gthread::threads.clear();
  }
  friend void start_gthreads();
  friend void gthread_yield();
  friend void gthread_terminate();
};





#endif
