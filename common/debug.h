#ifndef DEBUG_H
#define DEBUG_H

#include <iostream>

#ifdef DEBUG
  #define D_PRINT(x) std::cerr << "[DEBUG] (" << __FILE__ << ":" << __LINE__ << ") " << x << std::endl
#else
  #define D_PRINT(x)
#endif

#endif
