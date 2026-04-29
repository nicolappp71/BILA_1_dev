#include "vars.h"

static int32_t var_ = 0;

void set_var_(int32_t value) {
    var_ = value;
}

int32_t get_var_(void) {
    return var_;
}