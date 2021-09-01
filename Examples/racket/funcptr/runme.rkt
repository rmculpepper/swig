#lang racket/base
(require "example.rkt"
         rackunit)

(define a 37)
(define b 42)

;; Now call our C function with a bunch of callbacks

(check-equal? (do_op a b add) (+ a b))
(check-equal? (do_op a b sub) (- a b))
(check-equal? (do_op a b mul) (* a b))
