%module example

%insert("rktheader") %{
(define foreign-lib (ffi-lib "example.so"))
(define-ffi-definer define-foreign foreign-lib)
%}

// Define FIXME so that untranslated constants, for example, do not
// cause compile failure in Racket module.
%insert("rktheader") %{
(define (FIXME . args) (void))
%}

// ----------------------------------------
// Constant declarations

#define ONE 1
#define TEN 0xA
#define ELEVEN (ONE + TEN)
#define FIFTY (4 * ELEVEN + 6)

// ----------------------------------------
// Variable declarations

extern int counter;

extern const double pi;

// Use #:fail option to handle undefined symbols in shared lib.
%feature("var-options") undef_var "#:fail (lambda () #f)";

extern int undef_var;

// Use #:make-fail make-not-available to create a procedure that fails
// when applied and mentions the variable name in the error message.
%feature("var-options") undef_var2 "#:make-fail make-not-available";

extern int undef_var2;

// ----------------------------------------
// Function declarations

extern int get_counter();

extern bool counter_gte(int v);

// Use rktin and rktargout to implement in/out parameter.
// If there is any rktargout argument AND result is _void, result is omitted.
%typemap(rktin) int *INOUT "[$1_name : (_ptr io _int)]";
%typemap(rktargout) int *INOUT "INOUT";

extern void get_set_counter(int *INOUT);

// Use fun-result feature to set result expression.
%feature("fun-result") get_set_counter2 "(if (zero? result) INOUT #f)";

extern int get_set_counter2(int *INOUT);

// Use fun-options and #:fail or #:make-fail to handle undefined function symbol.
%feature("fun-options") undef_fun "#:make-fail make-not-available";

extern int undef_fun(int v);

// Use fun-prefix to set options like #:save-errno.
// Use multi-argument rktin typemap to handle strings. (Although
// sometimes, practically, it is better to keep the wrapper low-level
// and pass separate pointer and length arguments.)

%feature("fun-prefix") add_alpha_chars "#:save-errno 'posix";
%typemap("rktin") (char *s, int len) %{
  [$1_name : _string/utf-8]
  [$2_name : _int = (string-utf-8-length $1_name)]
%}
extern int add_alpha_chars(char *s, int len);

// ----------------------------------------
// Structs

%insert("rktheader") %{
(require racket/struct)
%}

// Use the struct-options feature to add properties like printing and
// equality implementations to the Racket struct wrapper.
%feature("struct-options") point_st %{
  #:property prop:custom-write
  (make-constructor-style-printer
   (lambda (self) 'Point)
   (lambda (self) (list (point_st-x self) (point_st-y self))))
  #:property prop:equal+hash
  (list (lambda (a b recur)
          (and (= (point_st-x a) (point_st-x b))
               (= (point_st-y a) (point_st-y b))))
        (lambda (p recur) 1)
        (lambda (p recur) 1))
%}

struct point_st {
  int x, y;
};

extern void reflect_point(struct point_st *p);

typedef struct point_st Point;

// Use Racket's ffi/unsafe/alloc library to automatically free
// allocated objects when the wrapping pointer is GC'd.
// %feature("fun-options") flip_point "#:wrap (allocator free)";
%newobject flip_point;

extern Point *flip_point(Point *p);

// Test for automatic deallocation.

extern int point_counter;

// %feature("fun-options") delete_point "#:wrap (deallocator)";
%delobject delete_point;
// %feature("fun-options") new_point "#:wrap (allocator delete_point)";
%newobject new_point;
%typemap(rktnewfree) Point * "delete_point";

extern void delete_point(Point *p);
extern Point *new_point();

// ----------------------------------------
// Pointers

// No typemap; treat as pointer.
extern void mul_intp(int *p, int factor);

// ----------------------------------------
// Enumerations

enum direction_t { north, east, south, west, up = 100 };

extern enum direction_t next_direction_cw(enum direction_t d);

// ----------------------------------------
// Unions

%feature("struct-options") thing_t %{
  #:property prop:custom-write
  (make-constructor-style-printer
   (lambda (self) 'thing)
   (lambda (self)
     (list (thing_t-t self)
      (case (thing_t-t self)
       [(point) (union-ref (thing_t-u self) 0)]
       [(direction) (union-ref (thing_t-u self) 1)]))))
%}

enum thing_tag_t { point, direction };
union thing_inner_t {
  Point p;
  enum direction_t d;
};
struct thing_t {
  enum thing_tag_t t;
  union thing_inner_t u;
};

extern void convert_thing(struct thing_t *t);
