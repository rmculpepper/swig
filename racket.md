# Racket

## Obligations

The interface module must insert into the "header" section a definition of
`foreign-lib` and `define-foreign`, where `foreign-lib` is a variable whose
value is the FFI library reference and `define-foreign` is a definition form
compatible with those produced by `define-ffi-definer`. For example:

    %insert("header") %{
    (define foreign-lib (ffi-lib "libexample.so"))
    (define-ffi-definer define-foreign foreign-lib
      #:default-make-fail make-not-available)
    %}


## Translations

### Defined constants

A `#define` constant is translated to an ordinary definition. Beware that if the
right-hand side is not a simple numeric or string literal, the expression will
not be well-formed.

### Variables

A variable is translated to

FIXME: const variables, don't set...

### Functions

A function is translated

## Types

The translation of C types to Racket FFI type descriptors is done by the
non-standard "ffi" typemap, but types that have no mapping are handled specially
by the translator. The special handling cannot be scripted by interface modules.

Racket's "ffi" typemap maps a C type to a Racket expression that evaluates to an
FFI type descriptor (satisfying the `ctype?` predicate).

FIXME: see also "in" typemap entries

### Builtin typemap

This section shows the built-in type mappings in Lib/racket/racket.swg:

    /* Integer types */

    %typemap(ffi) char "_byte";
    %typemap(ffi) signed char "_byte";
    %typemap(ffi) unsigned char "_ubyte";

    %typemap(ffi) wchar_t "_wchar"

    %typemap(ffi) short "_short";
    %typemap(ffi) signed short "_short";
    %typemap(ffi) unsigned short "_ushort";

    %typemap(ffi) int "_int";
    %typemap(ffi) signed int "_int";
    %typemap(ffi) unsigned int "_uint";

    %typemap(ffi) long "_long";
    %typemap(ffi) signed long "_long";
    %typemap(ffi) unsigned long "_ulong";

    %typemap(ffi) long long "_llong";
    %typemap(ffi) signed long long "_llong";
    %typemap(ffi) unsigned long long "_ullong";

    %typemap(ffi) intptr_t "_intptr";
    %typemap(ffi) uintptr_t "_uintptr";

    %typemap(ffi) int8_t "_int8"
    %typemap(ffi) uint8_t "_uint8"

    %typemap(ffi) int16_t "_int16"
    %typemap(ffi) uint16_t "_uint16"

    %typemap(ffi) int32_t "_int32"
    %typemap(ffi) uint32_t "_uint32"

    %typemap(ffi) int64_t "_int64"
    %typemap(ffi) uint64_t "_uint64"

    %typemap(ffi) size_t "_size"
    %typemap(ffi) ssize_t "_ssize"
    %typemap(ffi) ptrdiff_t "_ptrdiff"

    /* Floating-point types */

    %typemap(ffi) float "_float";
    %typemap(ffi) double "_double";
    %typemap(ffi) long double "_longdouble"

    /* Other atomic types */

    %typemap(ffi) bool "_stdbool"

    %typemap(ffi) void "_void";

    /* Pointer types */

    %typemap(ffi) void * "_pointer";
    %typemap(ffi) char * "_pointer";
    %typemap(ffi) unsigned char * "_pointer";

The default FFI type for `double` accepts only Racket floating-point numbers. To
automatically coerce other number (such as exact integers and rationals) to
floating point, use `_double*` instead by using the following declaration:

    // Include in your interface module:
    %typemap(ffi) double "_double*";

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

The definition also produces many additional names; see the documentation of
`define-cstruct` for details.

The translator records defined struct names; see the Typedefs section.

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

A C pointer type is translated differently according to the target type.

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

    int *               => (_pointer-to _int)



# Misc

## Out parameters

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

%typemap("in") int *outparam %{=
  [outparam : (_ptr o _int)]
%}
%feature("fun-result") f "outparam";

%feature("fun-prefix") f "(x y z) ::";
%feature("fun-result") f "(values $result outparam)";

Idea: use "in" typemap for parameters *only*; use "ffi" typemap for general conversions.
