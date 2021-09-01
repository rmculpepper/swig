/* File : example.i */
%module example

%header %{
(define foreign-lib (ffi-lib "example.so"))
(define-ffi-definer define-foreign foreign-lib)
%}

%rename(f_gcd) gcd;

extern int    gcd(int x, int y);
extern double Foo;
