/* File : example.i */
%module example

%{
(define foreign-lib (ffi-lib "example.so"))
(define-ffi-definer define-foreign foreign-lib)
%}

%include "example.h"
