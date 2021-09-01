#lang racket
(require "example.rkt"
         rackunit)

(check-equal? ICONST 42)
(check-equal? FCONST 2.1828)
(check-equal? CCONST (char->integer #\x))
(check-equal? CCONST2 (char->integer #\newline))
(check-equal? SCONST "Hello World")
(check-equal? SCONST2 "\"Hello World\"")
;; (check-equal? EXPR 37)
(check-equal? iconst 37)
(check-equal? fconst 3.14)

(check-equal? (identifier-binding #'EXTERN) #f)
(check-equal? (identifier-binding #'FOO) #f)
