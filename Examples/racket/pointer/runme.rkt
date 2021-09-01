#lang racket/base
(require "example.rkt"
         rackunit)

(check-equal? (add 1 2) 3)
(check-equal? (subtract 12 7) 5)
(check-equal? (call-with-values (lambda () (divide 42 9)) list)
              (call-with-values (lambda () (quotient/remainder 42 9)) list))
