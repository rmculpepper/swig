#lang racket/base
(require "example.rkt"
         rackunit)

(define v (make-Vector 1.0 2.0 3.0))
(define w (make-Vector 10.0 11.0 12.0))

(printf "I just created the following vectors\n")
v
w

;; Now call some of our functions

(printf "\nNow I'm going to compute the dot product\n")
(define d (dot_product v w))
(printf "dot product = ~v\n" d)

(check-equal? d 68.0)

;; Add the vectors together

(printf "\nNow I'm going to add the vectors together\n")
(define r (vector_add v w))
r

(check-equal? r (make-Vector 11.0 13.0 15.0))
