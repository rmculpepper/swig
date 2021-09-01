;; run with racket runme.rkt

#lang racket/base
(require "example.rkt")

;; Call our gcd() function

(define x 42)
(define y 105)
(define g (f_gcd x y)) ;; renamed, gcd is Racket builtin
(printf "The gcd of ~v and ~v is ~v\n" x y g)

;; Manipulate the Foo global variable

;; Output its current value
(printf "Foo = ~v\n" (Foo))

; Change its value
(Foo 3.1415926)

; See if the change took effect
(printf "Foo = ~v\n" (Foo))
