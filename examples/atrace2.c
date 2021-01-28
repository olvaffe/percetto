#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_INPUT | ATRACE_TAG_VIEW)
#include "atrace-compat.h"

void atrace2(void) {
  ATRACE_BEGIN(__func__);
  ATRACE_END();
}
