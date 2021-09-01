/* File : example.i */
%module example

%{
(require (only-in racket/struct make-constructor-style-printer))
(define foreign-lib (ffi-lib "example.so"))
(define-ffi-definer define-foreign foreign-lib)
%}

%feature("struct-options") Point %{
  #:property prop:custom-write
  (make-constructor-style-printer
   (lambda (self) (quote Point))
   (lambda (self) (list (Point-x self) (Point-y self))))
  #:property prop:equal+hash
  (let ()
   (define (equal-to? a b recur)
     (and (equal? (Point-x a) (Point-x b))
          (equal? (Point-y a) (Point-y b))))
   (define (hash-code a recur) 1) ;; FIXME
   (list equal-to? hash-code hash-code))
%}

%include "example.h"

#pragma SWIG nowarn=SWIGWARN_TYPEMAP_SWIGTYPELEAK

/* Some global variable declarations */
extern int              ivar;
extern short            svar;
extern long             lvar;
extern unsigned int     uivar;
extern unsigned short   usvar;
extern unsigned long    ulvar;
extern signed char      scvar;
extern unsigned char    ucvar;
extern char             cvar;
extern float            fvar;
extern double           dvar;
extern char            *strvar;
extern const char       cstrvar[];
extern int             *iptrvar;
extern char             name[256];

extern Point           *ptptr;
extern Point            pt;

/* Some read-only variables */

%immutable;

extern int  status;
extern char path[256];

%mutable;

/* Some helper functions to make it easier to test */
extern void  print_vars();
extern int  *new_int(int value);
extern Point *new_Point(int x, int y);
extern char  *Point_print(Point *p);
extern void  pt_print();

