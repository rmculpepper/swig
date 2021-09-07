#lang racket/base
(require "example.rkt"
         ffi/unsafe
         rackunit)

;; ----------------------------------------
;; Constants

(check-equal? ONE 1)
(check-equal? TEN 10)
(check-equal? ELEVEN 11)

;; ----------------------------------------
;; Variables

(check-pred procedure? counter)

(check-equal? (counter) 0)

(counter 10)
(check-equal? (counter) 10)

(check-pred procedure? pi)

(check-equal? (pi) 3.141592)

(check-exn #rx"immutable"
           (lambda () (pi 3)))

(check-equal? undef_var #f)

(check-exn #rx"undef_var2: implementation not found"
           (lambda () (undef_var2)))

;; ----------------------------------------
;; Functions

(check-equal? (get_counter) 10)

(check-equal? (counter_gte 5) #t)
(check-equal? (counter_gte 15) #f)

(begin
  (check-equal? (get_set_counter 55) 10)
  (check-equal? (counter) 55))

(begin
  (check-equal? (get_set_counter2 -12) #f)
  (check-equal? (counter) 55)

  (check-equal? (get_set_counter2 1) 55)
  (check-equal? (counter) 1))

(check-exn #rx"undef_fun: implementation not found"
           (lambda () (undef_fun 100)))

(check-equal? (add_alpha_chars "ABC")
              (apply + (map char->integer (string->list "ABC"))))

(begin
  (check-equal? (add_alpha_chars "DE567") -1)
  (check-equal? (saved-errno) (lookup-errno 'EINVAL)))

;; ----------------------------------------
;; Structs

(define p0 (make-point_st 0 0))
(define p1 (make-point_st 3 4))

(check-equal? p0 p0)
(check-equal? p1 p1)
(check-not-equal? p0 p1)
(check-equal? (format "~v" p0) "(Point 0 0)")
(check-equal? (format "~v" p1) "(Point 3 4)")

(reflect_point p1)
(check-equal? p1 (make-point_st -3 -4))

(define p2 (flip_point (make-point_st 10 10)))
(check-equal? p2 (make-point_st 10 -10))

(let ()
  ;; Create 100 points
  (define ps (for/list ([i 100]) (new_point)))
  (check-equal? (point_counter) 100)
  ;; Manually deallocate 10 of them (but keep dangling wrappers!)
  (for ([i 10] [p ps]) (delete_point p))
  (check-equal? (point_counter) 90)
  ;; Allow all of the wrappers to be GC'd
  (void/reference-sink ps) ;; after this, ps var is dead and list can be GC'd
  (collect-garbage)
  ;; Check that all have been collected (and not deleted twice)
  (check-equal? (point_counter) 0))

;; ----------------------------------------
;; Pointers

(let ()
  (define p (make-point_st 1 2)) ;; acts like instance or pointer, depending on use
  (mul_intp p 5)                    ;; &(p->x) == p
  (mul_intp (ptr-add p 1 _int) 10)  ;; &(p->y)
  (check-equal? p (make-point_st 5 20)))

(printf "Okay.\n")

;; ----------------------------------------
;; Enumerations

(check-equal? (cast 0 _int _direction_t) 'north)
(check-equal? (cast 'up _direction_t _int) 100)

(check-equal? (next_direction_cw 'north) 'east)
(check-equal? (next_direction_cw 'up) 'up)

;; ----------------------------------------
;; Unions

(define (make-union utype i v)
  (define blob (malloc (ctype-sizeof utype) 'atomic))
  (define uvalue (ptr-ref blob utype))
  (union-set! uvalue i v)
  uvalue)

(define t1 (make-thing_t 'point (make-union _thing_inner_t 0 (make-point_st 2 1))))
(define t2 (make-thing_t 'direction (make-union _thing_inner_t 1 'north)))
(let ([s1 (format "~v" t1)])
  (convert_thing t1)
  #;(printf "~a => ~v\n" s1 t1)
  (check-equal? (format "~v" t1) "(thing 'direction 'east)"))
(let ([s2 (format "~v" t2)])
  (convert_thing t2)
  #;(printf "~a => ~v\n" s2 t2)
  (check-equal? (format "~v" t2) "(thing 'point (Point 0 1))"))
