// Tests SWIG's handling of pass-by-value for complex datatypes
%module example

%{
(require (only-in racket/struct make-constructor-style-printer))
(define foreign-lib (ffi-lib "example.so"))
(define-ffi-definer define-foreign foreign-lib)
%}

%feature("struct-options") Vector %{
  #:property prop:custom-write
  (make-constructor-style-printer
   (lambda (self) (quote Vector))
   (lambda (self) (list (Vector-x self) (Vector-y self) (Vector-z self))))
  #:property prop:equal+hash
  (let ()
   (define (equal-to? a b recur)
     (and (equal? (Vector-x a) (Vector-x b))
          (equal? (Vector-y a) (Vector-y b))
          (equal? (Vector-z a) (Vector-z b))))
   (define (hash-code a recur) 1) ;; FIXME
   (list equal-to? hash-code hash-code))
%}

%include "example.h"

/* Some functions that manipulate Vectors by value */
extern double dot_product(Vector a, Vector b);
extern Vector vector_add(Vector a, Vector b);
