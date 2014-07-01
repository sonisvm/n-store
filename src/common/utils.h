#ifndef _UTILS_H_
#define _UTILS_H_

#include <vector>
#include <ctime>

#include "record.h"

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
  unsigned int num_columns = sptr->num_columns;
  unsigned int itr;
  std::string rec_str;

  for (itr = 0; itr < num_columns; itr++) {
    rec_str += rptr->get_data(itr, sptr) + " ";
  }

  return rec_str;
}

inline bool* get_key(vector<bool> key_vec) {
  size_t len = key_vec.size();

  bool* key = new bool[len];
  unsigned int itr = 0;

  for (itr = 0; itr < len; itr++)
    key[itr] = key_vec[itr];

  return key;
}

void simple_skew(vector<int>& zipf_dist, int n, int num_values);

void zipf(vector<int>& zipf_dist, double alpha, int n, int num_values);

void uniform(vector<double>& uniform_dist, int num_values);

void display_stats(timespec start, timespec finish, int num_txns);

void wrlock(pthread_rwlock_t* access);

void unlock(pthread_rwlock_t* access);

void rdlock(pthread_rwlock_t* access);

#endif /* UTILS_H_ */
