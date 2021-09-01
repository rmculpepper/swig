/* File : example.i */
%module example

%{
(define foreign-lib (ffi-lib "example.so"))
(define-ffi-definer define-foreign foreign-lib
  #:default-make-fail make-not-available)

(define _char (make-ctype _byte char->integer integer->char))
%}

%typemap(ffi) char "_char";

// %include exception.i
// %include typemaps.i

extern int    gcd(int x, int y);

%typemap(in) (int argc, char *argv[]) %{
  [$1_name : _int = (length $2_name)]
  [$2_name : (_list i _string)]
%}

extern int gcdmain(int argc, char *argv[]);

%typemap(in) (char *bytes, int len) %{
  [$1_name : _string/utf-8]
  [$2_name : _int = (string-utf-8-length $1_name)]
%}

extern int charcount(char *bytes, int len, char c);

/* This example shows how to wrap a function that mutates a string */

// This doesn't make sense for a string; byte-level mutation might
// change the length!  Use bytestring instead.

%feature("fun-prefix") capitalize "(rktstr) ::";
%typemap(in) (char *str, int len) %{
  [$1_name : _bytes = (string->bytes/utf-8 rktstr)]
  [$2_name : _int = (bytes-length $1_name)]
%}

%feature("fun-result") capitalize "(bytes->string/utf-8 str)";
// This alternative solution results in two result values:
// %typemap(argout) (char *str, int len) "(bytes->string/utf-8 $1_name)";

/* Return the mutated string as a new object.  */

extern void capitalize(char *str, int len);

/* A multi-valued constraint.  Force two arguments to lie
   inside the unit circle */

%feature("fun-options") circle %{
  #:wrap (lambda (f)
           (lambda (a b)
             (when (> (+ (* a a) (* b b)) 1.0)
               (error (quote $wrapname) "points must be in unit circle"))
             (f a b)))
%}
 
extern void circle (double cx, double cy);
extern int squareCubed (int n, int *OUTPUT);
