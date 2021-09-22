# Issues:

## defined in other lib

How should we communicate that a type is defined in another module?

MAYBE: extern "racket" { ??? }

MAYBE: %feature("pre-defined");
       struct foo;
       ...
       %clearfeature("pre-defined");

## other features

- "NoDefineType": assume type defn is imported; do not generate
  define-cstruct, etc

- "EnumMode": select between _enum and some integer type + constants

- "ExternAll": like -extern-all; emit wrappers for non-extern functions

- "ForwardMode": select between forward declaration modes
  - "lift":


## Lifting forward (and implicit) declarations

If a forward declaration is encountered, like

    struct foo;
    enum bar;

or if a forward reference is encountered w/o a declaration, then

- map the type to a fixup record containing the current output position,
  eg "struct foo" -> { "position" = 82 }
- append the type name to the fixup list

When a type declaration is encountered and there is a fixup record,
don't emit the type definition, but add it to the fixup record under
the "insert" key.
