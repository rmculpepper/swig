# To Do (Maybe)

- "DefineTypesMode"  -- macros %DefineAllTypes, %DefineNoTypes, %DefinePointerTypes
  - "all", unset -- define all
  - "none" -- define nothing (eg, assume imported)
  - "pointer" -- define only pointer type

- "ExternAll": like -extern-all; emit wrappers for non-extern functions

- "EnumMode": select between _enum and some integer type + constants
  - "enum", unset -- define as _enum, symbols
  - "_<integertype>" etc -- define as _<integertype>, define constants
