/* -----------------------------------------------------------------------------
 * racket.swg
 * ----------------------------------------------------------------------------- */

/* Define a C preprocessor symbol that can be used in interface files
   to distinguish between the SWIG language modules. */ 

#define SWIG_RACKET
%insert("header") %{
#include <string.h>
#include <stdlib.h>
%}

#define %DefineNoTypes      %feature("DefineTypesMode", "none")
#define %DefinePointerTypes %feature("DefineTypesMode", "pointer")
#define %DefineAllTypes     %feature("DefineTypesMode", "all")
#define %ClearDefineTypes   %feature("DefineTypesMode", "")

/* Integer types */

%typemap(rktffi) char "_byte";
%typemap(rktffi) signed char "_byte";
%typemap(rktffi) unsigned char "_ubyte";

%typemap(rktffi) wchar_t "_wchar"

%typemap(rktffi) short "_short";
%typemap(rktffi) signed short "_short";
%typemap(rktffi) unsigned short "_ushort";

%typemap(rktffi) int "_int";
%typemap(rktffi) signed int "_int";
%typemap(rktffi) unsigned int "_uint";

%typemap(rktffi) long "_long";
%typemap(rktffi) signed long "_long";
%typemap(rktffi) unsigned long "_ulong";

%typemap(rktffi) long long "_llong";
%typemap(rktffi) signed long long "_llong";
%typemap(rktffi) unsigned long long "_ullong";

%typemap(rktffi) intptr_t "_intptr";
%typemap(rktffi) uintptr_t "_uintptr";

%typemap(rktffi) int8_t "_int8"
%typemap(rktffi) uint8_t "_uint8"

%typemap(rktffi) int16_t "_int16"
%typemap(rktffi) uint16_t "_uint16"

%typemap(rktffi) int32_t "_int32"
%typemap(rktffi) uint32_t "_uint32"

%typemap(rktffi) int64_t "_int64"
%typemap(rktffi) uint64_t "_uint64"

%typemap(rktffi) size_t "_size"
%typemap(rktffi) ssize_t "_ssize"
%typemap(rktffi) ptrdiff_t "_ptrdiff"

// _intmax
// _uintmax

/* Floating-point types */

%typemap(rktffi) float "_float";
%typemap(rktffi) double "_double";
// %typemap(rktffi) double "_double*";

%typemap(rktffi) long double "_longdouble"

/* Other atomic types */

%typemap(rktffi) bool "_stdbool"

%typemap(rktffi) void "_void";

/* Pointer types */

%typemap(rktffi) void * "_pointer";
%typemap(rktffi) char * "_pointer";
%typemap(rktffi) unsigned char * "_pointer";

// %typemap(rktffi) const void * "_pointer";
// %typemap(rktffi) const char * "_pointer";
// %typemap(rktffi) const unsigned char * "_pointer";

/* ======================================== */
/* Code inserts */

%insert("rktheader") %{
#lang racket/base
(require ffi/unsafe ffi/unsafe/define ffi/unsafe/alloc)
(provide (protect-out (all-defined-out)))

(module ffi-type-util racket/base
  (require ffi/unsafe)
  (define (_pointer-to type) _pointer) ;; Hint: maybe use (_ptr ? type) instead
  (define (c-variable lib rktname name type
                      #:immutable? [immutable? #f]
                      #:make-fail [make-fail #f]
                      #:fail [fail (and make-fail (make-fail rktname))])
    (let/ec escape
      (void (ffi-obj-ref name lib (and fail (lambda () (escape (fail))))))
      (define p (make-c-parameter name lib type))
      (define (cannot-set) (error rktname "variable ~s is immutable" name))
      (case-lambda [() (p)] [(v) (if immutable? (cannot-set) (p v))])))
  (provide (protect-out (all-defined-out))))
(require (submod "." ffi-type-util))
%}
