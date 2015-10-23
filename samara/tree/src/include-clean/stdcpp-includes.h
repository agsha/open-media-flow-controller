/*
 *
 * stdcpp-includes.h
 *
 *
 *
 */

/*
 *  This list contains every header file in the C++ standard library.
 *
 *  The intented use is to include this in common.h, and then do a full
 *  build.  any conflicting symbols will then be detected.  It may also
 *  be interesting to include susv3-includes.h at the same time.
 *
 *  See also test/general/include_test.sh .
 */

#include <algorithm>
#include <bitset>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cfloat>
#include <ciso646>
#include <climits>
#include <clocale>
#include <cmath>
#include <complex>
#include <csetjmp>
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <cwctype>
#include <deque>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <istream>
#include <iterator>
#include <limits>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <new>
#include <numeric>
#include <ostream>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <typeinfo>
#include <utility>
#include <valarray>
#include <vector>

/* Our version of g++ seems to have these too */
#include <cxxabi.h>

/* Later C++ standard (not a complete list) */
#if 0
#include <array>
#include <ctype>
#include <cwstring>
#include <forward_list>
#include <unordered_map>
#include <unordered_set>
#endif
