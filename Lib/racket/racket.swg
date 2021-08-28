/* -----------------------------------------------------------------------------
 * racket.swg
 * ----------------------------------------------------------------------------- */

/* Define a C preprocessor symbol that can be used in interface files
   to distinguish between the SWIG language modules. */ 

#define SWIG_RACKET

/* Integer types */

%typemap(in) char "_byte";
%typemap(in) signed char "_byte";
%typemap(in) unsigned char "_ubyte";

%typemap(in) wchar_t "_wchar"

%typemap(in) short "_short";
%typemap(in) signed short "_short";
%typemap(in) unsigned short "_ushort";

%typemap(in) int "_int";
%typemap(in) signed int "_int";
%typemap(in) unsigned int "_uint";

%typemap(in) long "_long";
%typemap(in) signed long "_long";
%typemap(in) unsigned long "_ulong";

%typemap(in) long long "_llong";
%typemap(in) signed long long "_llong";
%typemap(in) unsigned long long "_ullong";

%typemap(in) intptr_t "_intptr";

%typemap(in) int8_t "_int8"
%typemap(in) uint8_t "_uint8"

%typemap(in) int16_t "_int16"
%typemap(in) uint16_t "_uint16"

%typemap(in) int32_t "_int32"
%typemap(in) uint32_t "_uint32"

%typemap(in) int64_t "_int64"
%typemap(in) uint64_t "_uint64"

%typemap(in) size_t "_size"
%typemap(in) ssize_t "_ssize"
%typemap(in) ptrdiff_t "_ptrdiff"

// _intmax
// _uintmax

/* Floating-point types */

%typemap(in) float "_float";
%typemap(in) double "_double";
// %typemap(in) double "_double*";

%typemap(in) long double "_longdouble"

/* Other atomic types */

%typemap(in) bool "_stdbool"

%typemap(in) void "_void";

/* Pointer types */

%typemap(in) void * "_pointer";
// %typemap(in) char * "_pointer";
// %typemap(in) unsigned char * "_pointer";

// %typemap(in) const void * "_pointer";
// %typemap(in) const char * "_pointer";
// %typemap(in) const unsigned char * "_pointer";

/* ======================================== */
/* Code inserts */

%insert("header") %{
#lang racket/base
(require ffi/unsafe ffi/unsafe/define)
(provide (protect-out (all-defined-out)))

(module ffi-type-util racket/base
  (require ffi/unsafe)
  (define (_pointer-to type) _pointer) ;; Hint: maybe use (_ptr ? type) instead
  (provide (protect-out (all-defined-out))))
(require (submod "." ffi-type-util))

(define-ffi-definer define-foreign
  FIXME)
%}
