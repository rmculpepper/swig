/* -----------------------------------------------------------------------------
 * racket.swg
 * ----------------------------------------------------------------------------- */

/* Define a C preprocessor symbol that can be used in interface files
   to distinguish between the SWIG language modules. */ 

#define SWIG_RACKET

/* Integer types */

%typemap(ffi) char "_byte";
%typemap(ffi) signed char "_byte";
%typemap(ffi) unsigned char "_ubyte";

%typemap(ffi) wchar_t "_wchar"

%typemap(ffi) short "_short";
%typemap(ffi) signed short "_short";
%typemap(ffi) unsigned short "_ushort";

%typemap(ffi) int "_int";
%typemap(ffi) signed int "_int";
%typemap(ffi) unsigned int "_uint";

%typemap(ffi) long "_long";
%typemap(ffi) signed long "_long";
%typemap(ffi) unsigned long "_ulong";

%typemap(ffi) long long "_llong";
%typemap(ffi) signed long long "_llong";
%typemap(ffi) unsigned long long "_ullong";

%typemap(ffi) intptr_t "_intptr";
%typemap(ffi) uintptr_t "_uintptr";

%typemap(ffi) int8_t "_int8"
%typemap(ffi) uint8_t "_uint8"

%typemap(ffi) int16_t "_int16"
%typemap(ffi) uint16_t "_uint16"

%typemap(ffi) int32_t "_int32"
%typemap(ffi) uint32_t "_uint32"

%typemap(ffi) int64_t "_int64"
%typemap(ffi) uint64_t "_uint64"

%typemap(ffi) size_t "_size"
%typemap(ffi) ssize_t "_ssize"
%typemap(ffi) ptrdiff_t "_ptrdiff"

// _intmax
// _uintmax

/* Floating-point types */

%typemap(ffi) float "_float";
%typemap(ffi) double "_double";
// %typemap(ffi) double "_double*";

%typemap(ffi) long double "_longdouble"

/* Other atomic types */

%typemap(ffi) bool "_stdbool"

%typemap(ffi) void "_void";

/* Pointer types */

%typemap(ffi) void * "_pointer";
%typemap(ffi) char * "_pointer";
%typemap(ffi) unsigned char * "_pointer";

// %typemap(ffi) const void * "_pointer";
// %typemap(ffi) const char * "_pointer";
// %typemap(ffi) const unsigned char * "_pointer";

/* ======================================== */
/* Code inserts */

%insert("header") %{
#lang racket/base
(require ffi/unsafe ffi/unsafe/define)
(provide (protect-out (all-defined-out)))

(module ffi-type-util racket/base
  (require ffi/unsafe)
  (define (_pointer-to type) _pointer) ;; Hint: maybe use (_ptr ? type) instead
  (define (c-variable lib rktname name type
                      #:immutable? [immutable? #f]
                      #:make-fail [make-fail #f]
                      #:fail [fail (and make-fail (make-fail rktname) #f)])
    (void (ffi-obj-ref name lib fail)) ;; for error-check
    (define p (make-c-parameter name lib type))
    (define (cannot-set) (error rktname "variable ~s is immutable" name))
    (case-lambda [() (p)] [(v) (if immutable? (cannot-set) (p v))]))
  (provide (protect-out (all-defined-out))))
(require (submod "." ffi-type-util))
%}
