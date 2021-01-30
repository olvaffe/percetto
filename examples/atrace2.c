#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_INPUT | ATRACE_TAG_VIEW)
#include "percetto-atrace.h"

void atrace2(void) {
  int64_t num = 8000000000;
  ATRACE_INT64("num", num);
  ATRACE_BEGIN(__func__);
  ATRACE_END();
}
