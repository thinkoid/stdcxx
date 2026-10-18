/* gcc's stddef.h spells offsetof as __builtin_offsetof, a keyword the
   EDG front end enters only when it emulates GNU, Microsoft or Clang,
   never in strict mode (github.com/edgcpp/compiler/discussions/185).
   __INTADDR__ is the front end's own offsetof primitive. */
#define __builtin_offsetof(T, M) ((__SIZE_TYPE__) __INTADDR__ (&((T *) 0)->M))
