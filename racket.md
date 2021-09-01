# Racket


## Obligations

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

A variable is translated to a procedure similar to those created by
`make-c-parameter`. Calling the procedure with zero arguments retrieves the
variable's current value, and calling it with one argument updates the
variable's value.

A variable wrapper can also be customized by the `%rename` directive and the
"var-options" feature, which can contain either `#:fail` or `#:make-fail`
keyword arguments, similar to those supported by the result of
`define-ffi-definer`. For example:

    %feature("var-options") undef_var "#:fail (lambda () #f)"
    int undef_var;

If the foreign library does not contain `undef_var`, the Racket `undef_var`
variable will get the value `#f` instead of a getter/setter procedure.

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

- The `-> $result-expr` is omitted if it is not overridden by the "fun-result"
  feature or "argout" typemap.
- The `[result : $result-type]` is simplified to `$result-type` if the `->
  $result-expr` is omitted.
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

For out (or inout) parameters, an idiomatic translation would use the `_ptr`
function parameter syntax (which is not, strictly speaking, an FFI type), as
follows:

    void f(int *outparam);
    =>
    (define-foreign f
      (_fun [outparam : (_ptr o _int)]
            -> _void
            -> outparam))

Without intervention, the default pointer type translation produces the
following Racket wrapper:

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


### Summary of wrapper options

FIXME: clarify
- code is expr vs list of clauses vs etc, other constraints
- what variables are bound in each block?

    %feature("fun-prefix") f "(x y z) ::";
    %feature("fun-result") f "(values $result outparam)";
