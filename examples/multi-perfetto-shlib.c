#include "percetto.h"
#include "multi-perfetto-shlib.h"

PERCETTO_CATEGORY_DEFINE(shlib, "Shared lib test events", 0);

bool test_shlib_init(void) {
  static struct percetto_category* categories[] = {
      PERCETTO_CATEGORY_PTR(shlib),
  };
  return percetto_init(sizeof(categories) / sizeof(categories[0]), categories);
}

void test_shlib_func(void) {
  TRACE_EVENT(shlib, "test_shlib_func");
}