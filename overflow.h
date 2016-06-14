#pragma once

#ifdef LACKS_BUILTIN_ADD_OVERFLOW
#warning "Get a real compiler"
/* Ugh, I should really include, say, the code from
 * https://www.fefe.de/intof.html, but I am assuming most people
 * (except Travis CI, of course) have at least GCC 5 these days. */
#define add_overflow(a,b,p) ({*p = a+b; 0;})
#else
#define add_overflow __builtin_add_overflow
#endif
