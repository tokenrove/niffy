#pragma once

#define CONCAT_HELPER(x,y) x##y
#define CONCAT(x,y) CONCAT_HELPER(x,y)
#define GENSYM(x) CONCAT(x, __COUNTER__)
#define UNUSED GENSYM(_) __attribute((unused))
