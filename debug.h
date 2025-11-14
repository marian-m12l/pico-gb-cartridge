#pragma once

#ifdef ENABLE_UART
#include <stdio.h>
#define DEBUGF(...) printf(__VA_ARGS__)
#else
#define DEBUGF(...)
#endif
