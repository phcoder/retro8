#include "slock_wrapper.h"

#ifdef USE_SLOCK_WRAPPER
slock_t *slock_new(void);
void slock_free(slock_t *lock);
void slock_lock(slock_t *lock);
void slock_unlock(slock_t *lock);
void slock_free(slock_t *lock);
bool slock_try_lock(slock_t *lock);

slock_wrap::slock_wrap() {
  slck = slock_new();
}

slock_wrap::~slock_wrap() {
  slock_free(slck);
}

void slock_wrap::lock() {
  slock_lock(slck);
}

void slock_wrap::unlock() {
  slock_unlock(slck);
}

bool slock_wrap::try_lock() {
  return slock_try_lock(slck);
}

#endif
