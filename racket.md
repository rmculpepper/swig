% Swig and Racket (FFI)

# Swig and Racket (FFI)

Run the Racket Swig translator using the following command:

    swig -racket -extern-all $interface-file.i

The output is a Racket module named `$module.rkt`, where $module is the module
name declared in the interface file (or overridden by the `-module $module`
command line option).

The interface module must insert into the "rktheader" section a definition of
`foreign-lib` and `define-foreign`, where `foreign-lib` is a variable whose
value is the FFI library reference and `define-foreign` is a definition form
compatible with those produced by `define-ffi-definer`. For example:

    %insert("rktheader") %{
    (define foreign-lib (ffi-lib "libexample.so"))
    (define-ffi-definer define-foreign foreign-lib
      #:default-make-fail make-not-available)
    %}


## Translations

### Type declarations

The translations of type declarations are discussed in the "Types" section.

### Constant definitions

A `#define` constant is translated to a Racket variable definition. If the
translator cannot handle the right-hand side (for example, if it is a nontrivial
arithmetic expression), it emits a Racket expression that calls a fictitious
`FIXME` Racket function.

### Variable declarations

A variable is translated to a procedure similar to the result of
`make-c-parameter`. Calling the procedure with zero arguments retrieves the
variable's current value, and calling it with one argument updates the
variable's value. If the variable is declared immutable (for example, with the
`%immutable` directive), calling the procedure to set the variable's value
raises an error.

A variable wrapper can also be customized by the `%rename` directive and the
"var-options" feature, which can contain either `#:fail` or `#:make-fail`
keyword arguments, similar to those supported by the result of
`define-ffi-definer`. For example:

    %feature("var-options") undef_var "#:fail (lambda () #f)"
    // %feature("var-options") undef_var "#:make-fail make-not-available"
    int undef_var;

If the foreign library does not contain `undef_var`, the Racket `undef_var`
variable will get the value `#f` instead of a getter/setter procedure. If the
commented-out feature is used instead, then `undef_var` is bound to a procedure
that always raises an error when applied.

### Function declarations

A function is translated to a procedure defined using `define-foreign` and a
`_fun` type. The formal parameters are translated to `_fun` argument clauses;
the default translation produces a clause of the following shape for each
parameter:

    [$argname : $ffitype]   // if the parameter name $argname is given
    $ffitype                // if the parameter name is not given

The translation of parameters to clauses can be overridden with the "rktin"
typemap, which also allows multi-parameter mappings. The mapping must produce as
many argument clauses as there are parameters in the C declaration. For example,
here is a multi-parameter typemap that converts a single Racket argument (a
list) to a pair of length and array arguments to the foreign function:

    %typemap(rktin) (int argc, char *argv[]) %{
      [$1_name : _int = (length $2_name)]
      [$2_name : (_list i _string)]
    %}

A function wrapper can also be customized by the `%rename` directive, the
"fun-prefix" feature, the "fun-result" feature, and the "fun-options"
feature. The following is the general template of a function wrapper:

    (define-foreign $rktname
      (_fun $fun-prefix
            $param-clause ...
            -> [result : $result-type]
            -> $result-expr)
      #:c-id $cname
      $fun-options)

- The `$result-expr` is determined as follows:
  - If there is a "fun-result" declaration, its value is used.
  - Otherwise, if there are any "rktargout" mappings, then `$result-expr` is
    `(values result $argout-expr ...)`, where each `$argout-expr` comes from a
    "rktargout" mapping, in the order that the arguments appear.
  - Otherwise, there is no `$result-expr`. In that case, the `-> $result-expr`
    line is omitted, and the `[result : $result-type]` line is simplified to
    just `$result-type`.
- The `#:c-id` declaration is omitted if there is no renaming.


## Types

The translation of C types to Racket FFI type descriptors is done by the
non-standard "rktffi" typemap, but types that have no mapping are handled
specially by the translator. The special handling cannot be scripted by
interface modules.

Racket's "rktffi" typemap maps a C type to a Racket expression that evaluates to
an FFI type descriptor (satisfying the `ctype?` predicate).

### Builtin typemap

This section shows the built-in type mappings in Lib/racket/racket.swg:

    /* Integer types */

    %typemap(rktffi) char "_byte";
    %typemap(rktffi) signed char "_byte";
    %typemap(rktffi) unsigned char "_ubyte";

    %typemap(rktffi) wchar_t "_wchar"

    %typemap(rktffi) short "_short";
    %typemap(rktffi) signed short "_short";
    %typemap(rktffi) unsigned short "_ushort";

    %typemap(rktffi) int "_int";
    %typemap(rktffi) signed int "_int";
    %typemap(rktffi) unsigned int "_uint";

    %typemap(rktffi) long "_long";
    %typemap(rktffi) signed long "_long";
    %typemap(rktffi) unsigned long "_ulong";

    %typemap(rktffi) long long "_llong";
    %typemap(rktffi) signed long long "_llong";
    %typemap(rktffi) unsigned long long "_ullong";

    %typemap(rktffi) intptr_t "_intptr";
    %typemap(rktffi) uintptr_t "_uintptr";

    %typemap(rktffi) int8_t "_int8"
    %typemap(rktffi) uint8_t "_uint8"

    %typemap(rktffi) int16_t "_int16"
    %typemap(rktffi) uint16_t "_uint16"

    %typemap(rktffi) int32_t "_int32"
    %typemap(rktffi) uint32_t "_uint32"

    %typemap(rktffi) int64_t "_int64"
    %typemap(rktffi) uint64_t "_uint64"

    %typemap(rktffi) size_t "_size"
    %typemap(rktffi) ssize_t "_ssize"
    %typemap(rktffi) ptrdiff_t "_ptrdiff"

    /* Floating-point types */

    %typemap(rktffi) float "_float";
    %typemap(rktffi) double "_double";
    %typemap(rktffi) long double "_longdouble"

    /* Other atomic types */

    %typemap(rktffi) bool "_stdbool"

    %typemap(rktffi) void "_void";

    /* Pointer types */

    %typemap(rktffi) void * "_pointer";
    %typemap(rktffi) char * "_pointer";
    %typemap(rktffi) unsigned char * "_pointer";

The default FFI type for `double` accepts only Racket floating-point numbers. To
automatically coerce other number (such as exact integers and rationals) to
floating point, use `_double*` instead by using the following declaration:

    // Include in your interface module:
    %typemap(rktffi) double "_double*";

### Struct types

A C struct definition is translated to a Racket `define-cstruct` definition:

    struct point_st { int x, y; }
    =>
    (define-cstruct _point_st
      ([x _int]
       [y _int]))

The Racket definition defines three FFI types:
- `_point_st` is a FFI type for instances of the structure; it corresponds to
  `struct point_st`
- `_point_st-pointer` is an FFI type for non-nullable pointers to instances; it
  corresponds to `struct point_st *` but disallows NULL
- `_point_st-pointer/null` is an FFI type for nullable pointers to instances; it
  corresponds to `struct point_st *` and allows NULL (represented by `#f`)

Racket pointers carry tags, and `_point_st-pointer/null` requires a pointer
argument to have the corresponding tag (`point_st-tag`, which has the value
`'point_st`). See the Racket FFI documentation for details.

The translator records known struct names; this information is used by the
translation of pointer types (see the "Pointer types" section), and it is
propagated by typedefs (see the "Typedefs" section).

### Typedefs

The handling of a typedef depends on whether the right-hand side is a known
struct type or not.

A typedef to a known struct type is handled as follows:

    typedef struct point_st Point;
    =>
    (define _Point              _point_st)
    (define _Point-pointer      _point_st-pointer)
    (define _Point-pointer/null _pointer_st-pointer/null)

In addition, the typedef name is recorded as a known struct type.

If the typedef occurs strictly before the declaration of the struct type, the
resulting Racket module will contain variable references before their
definitions. This can be fixed manually by moving the struct-generated
definition before the typedef-generated definitions.

A typedef to an anonymous struct declaration is handled as if the typedef name
replaced the struct name:

    typedef struct { int x, y, z; } Point3d;
    =>
    (define-cstruct _Point3d ([x _int] [y _int] [z _int]))

If the target of a typedef is not a known struct type, then the translation does
not include pointer types:

    typedef int errcode;
    =>
    (define _errcode _int)

### Union types

A C union declaration is translated to a Racket definition using `_union`:

    union shape_t { rectangle_t rect ; circle_t circ; };
    =>
    (define _shape_t (_union _rectangle_t _circle_t))

### Enumeration types

A C enumeration declaration is translated to a Racket definition using `_enum`:

    enum direction_t { north, east, south, west, up = 100 };
    =>
    (define _direction_t (_enum '(north east south west up = 100)))

### Pointer types

The translation of a C pointer type depends on the target type.

If the target type is typemapped to a Racket FFI type for a known struct type
(or typedef to a known struct type), then the associated nullable pointer type
is produced.

If the target type is a known struct type (or typedef to a known struct type),
then the associated nullable pointer type is produced.

    struct point_st *   =>  _point_st-pointer/null
    Point *             =>  _Point-pointer/null

If the target type is a function type, then the translation of the function type is produced.

Otherwise, an expression of the form `(_pointer-to target-type)` is produced,
where `target-type` is the translation of the target type. Note, however, that
`_pointer-to` is a constant function that always produces `_pointer`; the
`target-type` is included to aid manual editing of the resulting code.

    int *               =>  (_pointer-to _int)  =  _pointer

Qualifiers like `const` are discarded. The generated Racket code does not
respect mutation restrictions based on `const`.


## Misc notes

If the translator cannot translate an expression (such as the right-hand side of
a `#define`), it emits a call to an unbound `FIXME` Racket function.

If the translator cannot translate a type, it emits a call to an unbound
`_FIXME` function.

A "block" ... FIXME

### Out parameters


### Summary of wrapper options

FIXME: clarify
- code is expr vs list of clauses vs etc, other constraints
- what variables are bound in each block?

    %feature("fun-prefix") f "(x y z) ::";
    %feature("fun-result") f "(values $result outparam)";


## Supported and unsupported Swig features

Some of these are unsupported because they are unneeded.

Unsupported doesn't necessarily mean "raises an error"; it just means don't do that.

### Default arguments (5.4.8, 13.5)

Default arguments are not directly supported. That is, the following declaration
syntax is not supported:

    int plot(double x, double y, int color=WHITE);  // NOT SUPPORTED
    
Also, the "default" typemap method is not supported. (See the "Common typemap
method" section below.)

To create a wrapper with default arguments, use the "fun-prefix" feature to
specify the formal parameter list (ending in `::`) for the `_fun` syntax:

    %feature("fun-prefix") plot "(x y [color 0]) ::"
    int plot (double x, double y, int color);
    =>
    (define-foreign plot
      (_fun (x y [color 0]) ::
            [x : _double]
            [y : _double]
            [flags : _int]
            -> int))

### `%constant` functions, `%callback`, `%nocallback` (5.4.9)

    %constant int add(int, int);
    
In many cases, you can just use the ordinary binding of `add`.
FIXME: example

### `%extend` (5.5.6)

???

### Nested structures (5.5.7)

supported?

### Code insertion (5.6)

The standard `"begin"`, `"runtime"`, `"header"`, `"wrapper"`, and
`"init"` targets are not used.

Use `"rktbegin"`, `"rktheader"`, `"rktwrapper"`, `"rktinit"` instead.
- `rktbegin`: before the `#lang racket/base` line
- `rktheader`: before the wrappers; define `foreign-lib` and `define-foreign` here
- `rktwrapper`: wrappers are generated here
- `rktinit`: extra code after all the wrappers are defined, emitted at
  module top-level (that is, not within a function)

Use only the `"..."` and `"%{...%}"` notations for inserted code. Do
not use `{...}`.

Do not use `%inline`, since it emits code to the `"header"` target.

### Input, output, and input/output parameters (12.1.2, 12.1.3, 12.1.4)

For a function with out (or inout) parameters, an idiomatic wrapper
would use the special `_ptr` function parameter syntax (not an FFI
type), as follows:

    void f(int *outparam);
    =>
    (define-foreign f
      (_fun [outparam : (_ptr o _int)]
            -> _void
            -> outparam))

But by default, the translator produces the following Racket wrapper:

    (define-foreign f
      (_fun [outparam : (_pointer-to _int)]
            -> _void))

The argument can be treated as an out-parameter by using the "rktin" typemap to
override the default parameter clause to use the `(_ptr o ___)` argument syntax:

    %typemap(rktin) int *outparam %{
      [outparam : (_ptr o _int)]
    %}

And the result of the wrapper can be set using the "fun-result" feature:

    %feature("fun-result") f "outparam";

As an alternative, the argument can be automatically added to the function
result values by using the "argout" typemap. In this example, that would cause
`f` to return two values, the first of which is always `(void)`, so it makes
more sense to use the "fun-result" feature to replace the result expression
entirely.

    %typemap("rktargout") int *outparam "outparam";
    // %typemap("rktargout") int *outparam "$1_name";  // alternative

Parameters that are used for both input and output should use the
`(_ptr io _)` function parameter syntax instead. Likewise, parameters
that are only used for input should use the `(_ptr i _)` function
syntax.

FIXME: "typemap.i" but only works for primitive types??

### Typemap special variables (13.4.3)

The special variables are handled by Swig, so they are supported.

The `$typemap(method, typepattern)` macro is supported, but beware that the
Racket translator only uses typemaps for certain builtin base types.

### Common typemap methods (13.5)

The common typemap methods ("in", "out", etc) are not supported. 

The common "in" method has two Racket analogues:
- "rktin" maps parameters to `_fun` parameter clauses
- "rktffi" maps parameter types to Racket FFI types (for parameters where
  "rktin" did not match)
  
The common "argout" method has the Racket analogue "rktargout". It maps
parameters to Racket expressions to be included in the result values.

The common "out" method has the Racket analogue "rktffi".

The "default" typemap has no direct Racket analogue. See the "Default arguments"
section above.

The "check" method has no direct Racket analogue. Use a custom FFI type (see
`make-ctype`), use a wrapper (use the "fun-options" feature to give a `#:wrap`
argument to `_fun`), or perform the check at a higher level before calling the
wrapper function. (FIXME: example)

    %insert("rktheader") %{
    (define _pos-int
      (make-ctype _int
                  (lambda (v) (if (> v 0) v (error '_pos-int "not positive: ~e" v)))))
    %}
    %typemap(ffitype) int positive "_pos-int";
    int isqrt(int positive);
    =>
    (define-foreign bar
      (_fun [positive : _pos-int]
            -> _int))
            
To reject NULL pointers in arguments, where the target type is a structure type,
change the "rktffi" mapping of the parameter to use the `_structname-pointer`
type instead of the default `_structname-pointer/null` type.

The following typemap methods have no Racket analogue: "typecheck", "arginit",
"freearg", "newfree", "ret", "memberin", "varin", "varout", and "throws".

### Multi-argument typemaps (13.9)

The "rktin" method supports multi-argument type mappings. The mapping must
produce as many argument clauses as there are parameters in the C
declaration. For example, here is a multi-parameter typemap that converts a
single Racket argument (a list) to a pair of length and array arguments to the
foreign function:

    %typemap(rktin) (int argc, char *argv[]) %{
      [$1_name : _int = (length $2_name)]
      [$2_name : (_list i _string)]
    %}

### Object ownership, `%newobject`, `%delobject` (14.2)

The `%newobject` and `%delobject` directives are not supported. Use the
"fun-options" feature and Racket's `ffi/unsafe/alloc` library instead. For
example:

    %feature("fun-options") blah "#:wrap (allocator free)"
    Foo *blah() { return new Foo(); }

This will cause Racket to call the Racket `free` function on the allocated
object when the Racket cpointer wrapping it is garbage-collected. The the
documentation for `ffi/unsafe/alloc` for more information.

### Contracts, `%contract` (15)

Contracts are not supported at the FFI wrapper level.
FIXME

### Arithmetic expressions

Arithmetic expressions can occur in `#define` right-hand sides and array
bounds. The Racket translator only handles trivial expressions; it generates a
call to the fictitious `FIXME` function for non-trivial expressions that must be
fixed by hand.

### Other unsupported features

- C++ (6, 7, 8, 9)
- constraints, the `constraints.i` library (12.2)
- temporary/local variables introduced by typemaps (13)
- typemap fragments, `%fragment` (13.11)
- the run-time type checker (13.12)
- typemaps and overloading (13.13) (FIXME)
- `%exception` (14.1)
- variable-length arguments, `%varargs` (16)
