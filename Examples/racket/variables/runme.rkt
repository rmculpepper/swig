#lang racket/base
(require "example.rkt"
         ffi/unsafe
         rackunit)

;; Try to set the values of some global variables

(define (new-int v)
  (define obj (malloc 'raw _int))
  (ptr-set! obj _int v)
  obj)

(define (copy-bytes bs)
  (define obj (malloc 'raw _byte (add1 (bytes-length bs))))
  (memmove obj bs (bytes-length bs))
  (ptr-set! (ptr-add obj (bytes-length bs)) _byte 0)
  obj)

(ivar     42)
(svar     -31000)
(lvar     65537)
(uivar    123456)
(usvar    61000)
(ulvar    654321)
(scvar    -13)
(ucvar    251)
(cvar     (char->integer #\S))
(fvar     3.14159)
(dvar     2.1828)
(iptrvar  (new-int 37))
(ptptr    (make-Point 37 42))

;;(strvar   "Hello World")
(strvar (copy-bytes #"Hello World"))

;;(name    "Bill")
(memmove (array-ptr (name)) #"Bill\0" 5)


;; Now print out the values of the variables

(printf "Variables (values printed from Racket)\n")

(printf "ivar      = ~v\n" (ivar))
(printf "svar      = ~v\n" (svar))
(printf "lvar      = ~v\n" (lvar))
(printf "uivar     = ~v\n" (uivar))
(printf "usvar     = ~v\n" (usvar))
(printf "ulvar     = ~v\n" (ulvar))
(printf "scvar     = ~v\n" (scvar))
(printf "ucvar     = ~v\n" (ucvar))
(printf "fvar      = ~v\n" (fvar))
(printf "dvar      = ~v\n" (dvar))
(printf "cvar      = ~v\n" (cvar))
(printf "strvar    = ~v\n" (cast (strvar) _pointer _string))
(printf "cstrvar   = ~v\n" (cstrvar))
(printf "iptrvar   = ~v\n" (iptrvar))
(printf "name      = ~v\n" (name))
(printf "ptptr     = ~v\n" (ptptr))
(printf "pt        = ~v\n" (pt))

(printf "\nVariables (values printed from C)\n")
(print_vars)

(printf "\nI'm going to try and update a structure variable.\n")

(pt (ptptr))

(printf "The new value is ~v\n" (pt))

(printf "\nNow I'm going to try and modify some read only variables\n")

(check-exn #rx"immutable"
           (lambda ()
             (printf "  Trying to set 'status'\n")
             (status 0)))

;; path is immutable, but (memmove (array-ptr (path)) ___) succeeds

(printf "Done.\n")
