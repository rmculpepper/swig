/* File : example.i */
%module example

%{
(define foreign-lib (ffi-lib "example.so"))
(define-ffi-definer define-foreign foreign-lib)
%}

%typemap(in) int *INPUT "[$1_name : (_ptr i _int)]";

%typemap(in) int *OUTPUT "[$1_name : (_ptr o _int)]";
%typemap(argout) int *OUTPUT "$1_name";

%apply int *INPUT { int *x };
%apply int *INPUT { int *y };
%apply int *OUTPUT { int *z };

%feature("fun-result") add "z";
extern void add(int *x, int *y, int *z);

%feature("fun-result") subtract "OUTPUT";
extern void subtract(int *x, int *y, int *OUTPUT);

%apply int *OUTPUT { int *r };
extern int divide(int n, int d, int *r);
