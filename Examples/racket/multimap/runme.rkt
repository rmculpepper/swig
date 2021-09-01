;; run with mzscheme -r runme.scm

#lang racket/base
(require rackunit
         "example.rkt")

; Call the GCD function

(define x 42)
(define y 105)
(define g (gcd x y))

(printf "The gcd of ~v and ~v is ~v\n" x y g)

;  Call the gcdmain() function
(gcdmain '("gcdmain" "42" "105"))

(displayln (charcount "Hello World" #\l))

(displayln (capitalize "hello world"))

(circle 0.0 1.0)

(check-exn #rx"must be in unit circle"
           (lambda () (circle 3 4.0)))
