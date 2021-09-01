/* File : example.i */
%module example

%insert("rktheader") %{
(define foreign-lib (ffi-lib "example.so"))
(define-ffi-definer define-foreign foreign-lib)
%}

%rename(f_gcd) gcd;

extern int    gcd(int x, int y);
extern double Foo;
