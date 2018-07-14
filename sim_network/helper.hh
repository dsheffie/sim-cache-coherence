#ifndef __HELPERFUNCTS__
#define __HELPERFUNCTS__
#include <string>
#include <sstream>
#include <vector>
#include <cstdint>
#include <iostream>
#include <type_traits>

#include <cstdio>
#include <cstdarg>

static const char KNRM[] = "\x1B[0m";
static const char KRED[] = "\x1B[31m";
static const char KGRN[] = "\x1B[32m";
static const char KYEL[] = "\x1B[33m";
static const char KBLU[] = "\x1B[34m";
static const char KMAG[] = "\x1B[35m";
static const char KCYN[] = "\x1B[36m";
static const char KWHT[] = "\x1B[37m";

/* from gdb simulator */
static const uint32_t RSVD_INSTRUCTION  = 0x00000005;
static const uint32_t RSVD_INSTRUCTION_MASK = 0xFC00003F;
static const uint32_t RSVD_INSTRUCTION_ARG_SHIFT = 6;
static const uint32_t RSVD_INSTRUCTION_ARG_MASK = 0xFFFFF;
static const uint32_t IDT_MONITOR_BASE = 0xBFC00000;
static const uint32_t IDT_MONITOR_SIZE = 2048;
static const uint32_t K1SIZE = 0x80000000;

void dbt_backtrace();

#define die() {								\
    std::cerr << __PRETTY_FUNCTION__ << " @ " << __FILE__ << ":"	\
	      << __LINE__ << " called die\n";				\
    dbt_backtrace();							\
    abort();								\
  }



double timestamp();

uint32_t update_crc(uint32_t crc, uint8_t *buf, size_t len);
uint32_t crc32(uint8_t *buf, size_t len);

int32_t remapIOFlags(int32_t flags);

template <class T>
std::string toString(T x) {
  std::stringstream ss;
  ss << x;
  return ss.str();
}

template <class T>
std::string toStringHex(T x) {
  std::stringstream ss;
  ss << std::hex << x;
  return ss.str();
}

#define BS_PRED(SZ) (std::is_integral<T>::value && (sizeof(T)==SZ))
template <typename T, typename std::enable_if<BS_PRED(1),T>::type* = nullptr>
T bswap(T x) {
  return x;
}
template <typename T, typename std::enable_if<BS_PRED(2),T>::type* = nullptr>
T bswap(T x) {
  return __builtin_bswap16(x);
}
template <typename T, typename std::enable_if<BS_PRED(4),T>::type* = nullptr>
T bswap(T x) {
  return __builtin_bswap32(x);
}
template <typename T, typename std::enable_if<BS_PRED(8),T>::type* = nullptr>
T bswap(T x) {
  return __builtin_bswap64(x);
}

template <class T> bool isPow2(T x) {
  return (((x-1)&x) == 0);
}

#ifdef  __FreeBSD__
/* cribbed straight out of the FreeBSD source */
inline int dprintf(int fd, const char * __restrict fmt, ...) {
  va_list ap;
  int ret;
  va_start(ap, fmt);
  ret = vdprintf(fd, fmt, ap);
  va_end(ap);
  return (ret);
}
#endif


#endif
