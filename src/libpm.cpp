// libpm (perf)

#include "libpm.h"
#include <malloc.h>
#include <mutex>

using namespace std;

void* pmp;
struct static_info* sp;

// Global new and delete

void* operator new(size_t sz) throw (bad_alloc) {
  void* ret = calloc(1, sz);
  return ret;
}

void operator delete(void *p) throw () {
  free(p);
}

void *operator new[](std::size_t sz) throw (std::bad_alloc) {
  void* ret = calloc(1, sz);
  return ret;
}

void operator delete[](void *p) throw () {
  free(p);
}

void pmemalloc_free(void *abs_ptr_) {
  free(abs_ptr_);
}

void* pmemalloc_init(__attribute__((unused)) const char *path,
                     __attribute__((unused))            size_t size) {
  pmp = (void*) 1;
  return pmp;
}

void* pmemalloc_static_area() {
  void* static_area = new static_info();
  return static_area;
}

void pmemalloc_activate(void *abs_ptr_) {
  size_t len = malloc_usable_size(abs_ptr_);
  pmem_persist(abs_ptr_, len, 0);
}

void pmemalloc_end(const char *path) {
  FILE* rss_fp = NULL;
  long rss = 0L;
  long pmem_size;

  /*
  char cmd[1024];
  std::sprintf(cmd, "cat /proc/%d/status", getpid());
  int ret = system(cmd);
  if(ret == -1){
    perror("system");
  }
  */

  if ((rss_fp = fopen("/proc/self/statm", "r")) == NULL)
    return;

  if (fscanf(rss_fp, "%*s%ld", &rss) != 1) {
    fclose(rss_fp);
    return;
  }
  fclose(rss_fp);

  pmem_size = (size_t) rss * (size_t) sysconf( _SC_PAGESIZE);

  FILE* pmem_file = fopen(path, "w");
  if (pmem_file != NULL) {
    fprintf(pmem_file, "Active %lu \n", pmem_size);
    fclose(pmem_file);
  }

}

void pmemalloc_check(const char *path) {
  FILE* pmem_file = fopen(path, "r");
  long pmem_size;

  if (pmem_file != NULL) {
    int ret = fscanf(pmem_file, "Active %lu \n", &pmem_size);
    if (ret >= 0) {
      fprintf(stdout, "Active %lu \n", pmem_size);
    } else {
      perror("fscanf");
    }
    fclose(pmem_file);
  }
}
