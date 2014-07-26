#ifndef _UTILS_H_
#define _UTILS_H_

#include <vector>
#include <ctime>
#include <sstream>

#include "record.h"
#include "engine.h"

using namespace std;

// UTILS

inline std::string random_string(size_t len) {
  static const char alphanum[] = "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";

  char rep = alphanum[rand() % (sizeof(alphanum) - 1)];
  std::string str(len, rep);
  return str;
}

inline std::string get_data(record* rptr, schema* sptr) {
  std::string rec_str;

  if (rptr == NULL || sptr == NULL)
    return rec_str;

  unsigned int num_columns = sptr->num_columns;
  unsigned int itr;

  for (itr = 0; itr < num_columns; itr++) {
    if (sptr->columns[itr].enabled)
      rec_str += rptr->get_data(itr);
  }

  return rec_str;
}

// record* to string
std::string serialize(record* rptr, schema* sptr, bool prefix);

// string to record*
record* deserialize_to_record(std::string entry, schema* sptr, bool prefix);

// string to string
std::string deserialize_to_string(std::string entry_str, schema* sptr, bool prefix);

// szudzik hasher
inline unsigned long hasher(unsigned long a, unsigned long b, unsigned long c) {
  unsigned long a_sq = a * a;
  unsigned long ret = (a_sq + b) * (a_sq + b) + c;
  return ret;
}

void simple_skew(vector<int>& zipf_dist, int n, int num_values);

void zipf(vector<int>& zipf_dist, double alpha, int n, int num_values);

void uniform(vector<double>& uniform_dist, int num_values);

void display_stats(engine* ee, timeval* tv, int num_txns);

void wrlock(pthread_rwlock_t* access);

void unlock(pthread_rwlock_t* access);

void rdlock(pthread_rwlock_t* access);

#endif /* UTILS_H_ */
