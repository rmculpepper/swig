/* -----------------------------------------------------------------------------
 * This file is part of SWIG, which is licensed as a whole under version 3 
 * (or any later version) of the GNU General Public License. Some additional
 * terms also apply to certain portions of SWIG. The full details of the SWIG
 * license and copyrights can be found in the LICENSE and COPYRIGHT files
 * included with the SWIG source code as distributed by the SWIG developers
 * and at http://www.swig.org/legal.html.
 *
 * racket.cxx
 *
 * Racket language module for SWIG.
 * ----------------------------------------------------------------------------- */

#include <ctype.h>
#include "swigmod.h"

/* TODO:
 * - add options to _fun: prefix options, result expr, etc
 * - function options:
 *   - general prefix options, eg "#:save-errno 'posix"
 *   - fixed value for argument, eg "[x : _int = 0]"
 *   - result expression, eg "... -> [result : _int] -> (values (zero? int) outparam)"
 *   - specify and handle out & inout params: "(_ptr o type)", etc
 * - better, more consistent C->Racket expression handling
 *   - detect #define right-hand sides that are not legal Racket
 *     eg, "#define X (Y|0x01)"
 *   - array dimensions
 * - allow changing `define-foreign` name?
 * - allow customizing provided names?
 *   or put wrappers in submodule, let interface file re-export, add contracts, etc?
 * - fix memory leaks
 */

static const char *usage = "\
Racket Options (available with -racket)\n\
     -extern-all       - Create Racket definitions for all the functions and\n\
                         global variables otherwise only definitions for\n\
                         externed functions and variables are created.\n\
";

class RACKET:public Language {
public:
  File *f_rkt;
  File *f_begin;
  File *f_runtime;
  File *f_header;
  File *f_wrappers;
  File *f_init;

  String *module;
  virtual void main(int argc, char *argv[]);
  virtual int top(Node *n);
  virtual int functionWrapper(Node *n);
  virtual int variableWrapper(Node *n);
  virtual int constantWrapper(Node *n);
  virtual int classDeclaration(Node *n);
  virtual int classforwardDeclaration(Node *n);
  virtual int enumDeclaration(Node *n);
  virtual int typedefHandler(Node *n);
  List *entries;
private:
  String *adjust_param_tm(String *tm, int indent);
  String *get_ffi_type(Node *n, SwigType *ty);
  String *get_mapped_type(Node *n, SwigType *ty);
  void write_wrapper_options(File *f_out, String *opts, String *ffiname, String *cname);
  String *convert_literal(String *num_param, String *type);
  String *strip_parens(String *string);
  String *stringOfFunction(Node *n, ParmList *pl, String *restype, int indent);
  String *stringOfUnion(Node *n, int indent);
  void writeIndent(String *out, int indent, int moreindent);
  void add_known_type(String *ty, const char *kind);
  int is_known_struct_type(String *ty);
  void emit_forward_structs();
  int extern_all_flag;
  Hash *known_types;
  Hash *used_structs;
  Hash *defined_structs;
};

void RACKET::main(int argc, char *argv[]) {
  int i;

  Preprocessor_define("SWIGRACKET 1", 0);
  SWIG_library_directory("racket");
  SWIG_config_file("racket.swg");
  extern_all_flag = 0;
  known_types = NewHash();
  used_structs = NewHash();
  defined_structs = NewHash();

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-help")) {
      Printf(stdout, "%s\n", usage);
    } else if ((Strcmp(argv[i], "-extern-all") == 0)) {
      extern_all_flag = 1;
      Swig_mark_arg(i);
    }
  }
}

void RACKET::add_known_type(String *type, const char *kind) {
  Printf(stderr, "Adding known type: %s :: %s\n", type, kind);
  Setattr(known_types, type, kind);
}

int RACKET::is_known_struct_type(String *ffitype) {
  return checkAttribute(known_types, ffitype, "struct");
}

int RACKET::top(Node *n) {

  module = Getattr(n, "name");
  String *output_filename;
  entries = NewList();

  output_filename = NewStringf("%s%s.rkt", SWIG_output_directory(), module);
  f_rkt = NewFile(output_filename, "w+", SWIG_output_files());
  if (!f_rkt) {
    FileErrorDisplay(output_filename);
    SWIG_exit(EXIT_FAILURE);
  }

  f_begin = NewString("");
  f_runtime = NewString("");
  f_header = NewString("");
  f_wrappers = NewString("");
  f_init = NewString("");

  /* Register file targets with the SWIG file handler */
  Swig_register_filebyname("begin", f_begin);
  Swig_register_filebyname("runtime", f_runtime);
  Swig_register_filebyname("header", f_header);
  Swig_register_filebyname("wrapper", f_wrappers);
  Swig_register_filebyname("init", f_init);

  Swig_banner_target_lang(f_begin, ";;");

  Language::top(n);
  emit_forward_structs();

  Dump(f_begin, f_rkt);
  // Dump(f_runtime, f_rkt);     // swig.swg assumes runtime is C/C++; so omit
  Dump(f_header, f_rkt);
  Dump(f_wrappers, f_rkt);
  Dump(f_init, f_rkt);

  Delete(f_begin);      f_begin = NULL;
  Delete(f_runtime);    f_runtime = NULL;
  Delete(f_header);     f_header = NULL;
  Delete(f_wrappers);   f_wrappers = NULL;
  Delete(f_init);       f_init = NULL;
  Delete(f_rkt);        f_rkt = NULL;
  return SWIG_OK;
}


int RACKET::functionWrapper(Node *n) {
  String *storage = Getattr(n, "storage");
  if (!extern_all_flag && (!storage || (!Swig_storage_isextern(n) && !Swig_storage_isexternc(n))))
    return SWIG_OK;

  String *func_name = Getattr(n, "sym:name");
  String *cname = Getattr(n, "name");
  Append(entries, func_name);

  ParmList *pl = Getattr(n, "parms");
  Swig_typemap_attach_parms("in", pl, 0);

  String *restype = get_ffi_type(n, Getattr(n, "type"));
  String *expr = stringOfFunction(n, pl, restype, 2);
  Printf(f_wrappers, "(define-foreign %s\n", func_name);
  Printf(f_wrappers, "  %s", expr);
  if (Strcmp(func_name, cname)) {
    Printf(f_wrappers, "\n  #:c-id %s", cname);
  }
  write_wrapper_options(f_wrappers, Getattr(n, "feature:fun-options"), func_name, cname);
  write_wrapper_options(f_wrappers, Getattr(n, "feature:wrap-options"), func_name, cname);
  Printf(f_wrappers, ")\n\n");
  Delete(expr);
  Delete(restype);

  return SWIG_OK;
}

void RACKET::write_wrapper_options(File *f_out, String *opts, String *ffiname, String *cname) {
  if (opts && Strcmp(opts, "")) {
    // Each line of opts SHOULD be indented two spaces.
    // The last non-whitespace line of opts MUST NOT end in a line comment.
    opts = Copy(opts);
    Chop(opts);
    if (ffiname) Replaceall(opts, "$wrapname", ffiname);
    if (cname) Replaceall(opts, "$name", cname);
    if (Strncmp(opts, "\n", strlen("\n"))) {
      Printf(f_out, "\n");
    }
    Printf(f_out, "%s", opts);
    Delete(opts);
  }
}

int RACKET::constantWrapper(Node *n) {
  String *type = Getattr(n, "type");
  String *converted_value = convert_literal(Getattr(n, "value"), type);
  String *name = Getattr(n, "sym:name");

  Printf(f_wrappers, "(define %s %s)\n\n", name, converted_value);
  Append(entries, name);
  Delete(converted_value);

  return SWIG_OK;
}

int RACKET::variableWrapper(Node *n) {
  String *storage = Getattr(n, "storage");
  if (!extern_all_flag && (!storage || (!Swig_storage_isextern(n) && !Swig_storage_isexternc(n))))
    return SWIG_OK;

  String *var_name = Getattr(n, "sym:name");
  String *cname = Getattr(n, "name");
  String *lisp_type = get_ffi_type(n, Getattr(n, "type"));

  if (0) {
    Printf(f_wrappers, "(define-foreign %s %s", var_name, lisp_type);
    if (Strcmp(var_name, cname)) {
      Printf(f_wrappers, "\n  #:c-id %s", cname);
    }
    write_wrapper_options(f_wrappers, Getattr(n, "feature:var-options"), var_name, cname);
    write_wrapper_options(f_wrappers, Getattr(n, "feature:wrap-options"), var_name, cname);
    Printf(f_wrappers, ")\n\n");
  } else {
    Printf(f_wrappers, "(define %s (make-c-parameter \"%s\" foreign-lib %s))\n\n",
           var_name, cname, lisp_type);
  }

  Append(entries, var_name);

  Delete(lisp_type);
  return SWIG_OK;
}

int RACKET::typedefHandler(Node *n) {
  /* A declaration like "typedef struct foo_st { ... } FOO;" generates two things:
   * (1) a struct/class declaration for FOO (not foo_st!)
   *     More precisely: name="foo_st", sym:name="FOO", tdname="FOO".
   * (2) a typedef declaration for FOO
   *     More precisely: name="FOO", type="struct foo_st"
   * The struct/class decl is processed first, so we omit the typedef if the
   * given type name is already known.
   */
  String *tdtype = NewStringf("_%s", Getattr(n, "name"));
  if (!Getattr(known_types, tdtype)) {
    SwigType *ty = Getattr(n, "type");
    String *ffitype = get_ffi_type(n, ty);

    if (is_known_struct_type(ffitype)) {
      Printf(f_wrappers, "(define %s %s)\n", tdtype, ffitype);
      Printf(f_wrappers, "(define %s-pointer %s-pointer)\n", tdtype, ffitype);
      Printf(f_wrappers, "(define %s-pointer/null %s-pointer/null)\n", tdtype, ffitype);
      Printf(f_wrappers, "\n");
      add_known_type(tdtype, "struct");
    } else if (SwigType_issimple(ty) && !Strncmp(ty, "struct ", strlen("struct "))) {
      String *name = NewString(Char(ty) + strlen("struct "));
      if (1) {
        // If the struct is not declared (by C), definitions of ffitype etc are
        // inserted *before* this definition, which is okay.  If the struct is
        // declared later, this causes a use-before-def of ffitype etc, which
        // could be fixed by reordering the definitions.
        Printf(f_wrappers, "(define %s %s)\n", tdtype, ffitype);
        Printf(f_wrappers, "(define %s-pointer %s-pointer)\n", tdtype, ffitype);
        Printf(f_wrappers, "(define %s-pointer/null %s-pointer/null)\n", tdtype, ffitype);
        Setattr(used_structs, name, "1");
      } else {
        Printf(f_wrappers, "(define %s (begin _void #| incomplete type |#))\n", tdtype);
        Printf(f_wrappers, "(define %s-pointer (_cpointer '%s))\n", tdtype, name);
        Printf(f_wrappers, "(define %s-pointer/null (_cpointer/null '%s))\n", tdtype, name);
      }
      Printf(f_wrappers, "\n");
      add_known_type(tdtype, "struct");
    } else {
      add_known_type(tdtype, "typedef");
    }
  }
  return Language::typedefHandler(n);
}

int RACKET::enumDeclaration(Node *n) {
  if (getCurrentClass() && (cplus_mode != PUBLIC))
    return SWIG_NOWRAP;

  String *tyname = NewStringf("_%s", Getattr(n, "sym:name"));

  Printf(f_wrappers, "(define %s\n", tyname);
  Printf(f_wrappers, "  (_enum '(");

  int first = 1;

  for (Node *c = firstChild(n); c; c = nextSibling(c)) {
    String *slot_name = Getattr(c, "name");
    String *value = Getattr(c, "enumvalue");
    if (!first) { Printf(f_wrappers, " "); } else { first = 0; }
    if (value && !Strcmp(value, "")) {
      Printf(f_wrappers, "%s = %s", slot_name, value);
    } else {
      Printf(f_wrappers, "%s", slot_name);
    }
    Append(entries, slot_name);
    Delete(value);
  }

  Printf(f_wrappers, ")))\n\n");
  add_known_type(tyname, "enum");
  return SWIG_OK;
}

int RACKET::classforwardDeclaration(Node *n) {
  String *name = Getattr(n, "sym:name");
  String *kind = Getattr(n, "kind");
  // Printf(stderr, "forward declaration of %s :: %s\n", name, kind);
  if (!Strcmp(kind, "struct")) {
    Setattr(used_structs, name, "1");
  }
  return SWIG_OK;
}

void RACKET::emit_forward_structs() {
  Iterator iter;
  int first = 1;
  for (iter = First(used_structs); iter.item; iter = Next(iter)) {
    if (!Getattr(defined_structs, iter.key)) {
      if (first) {
        first = 0;
        Printf(f_header, "(module forward-struct-types racket/base\n");
        Printf(f_header, "  (require ffi/unsafe)\n");
        Printf(f_header, "  (provide (protect-out (all-defined-out)))");
      }
      // Printf(stderr, "declaring forward struct: %s\n", iter.key);
      Printf(f_header, "\n\n");
      Printf(f_header, "  (define _%s (begin _void #| incomplete type |#))\n", iter.key);
      Printf(f_header, "  (define _%s-pointer (_cpointer '%s))\n", iter.key, iter.key);
      Printf(f_header, "  (define _%s-pointer/null (_cpointer/null '%s))", iter.key, iter.key);
    }
  }
  if (!first) {
    Printf(f_header, ")\n(require (submod \".\" forward-struct-types))\n\n");
  }
}

// Includes structs
int RACKET::classDeclaration(Node *n) {
  String *name = Getattr(n, "sym:name");
  String *tyname = NewStringf("_%s", name);
  String *kind = Getattr(n, "kind");

  if (!Strcmp(kind, "struct")) {
    Append(entries, NewStringf("make-%s", name));
    Setattr(defined_structs, Getattr(n, "name"), "1");

    Printf(f_wrappers, "(define-cstruct %s\n", tyname);
    Printf(f_wrappers, "  (");

    int first = 1;

    for (Node *c = firstChild(n); c; c = nextSibling(c)) {
      if (Strcmp(nodeType(c), "cdecl")) {
        Printf(stderr, "Structure %s has a slot that we can't deal with.\n", name);
        Printf(stderr, "nodeType: %s, name: %s, type: %s\n",
               nodeType(c), Getattr(c, "name"), Getattr(c, "type"));
        SWIG_exit(EXIT_FAILURE);
      }

      String *temp = Copy(Getattr(c, "decl"));
      if (temp) {
        Append(temp, Getattr(c, "type"));	//appending type to the end, otherwise wrong type
        String *lisp_type = get_ffi_type(n, temp);
        Delete(temp);

        String *slot_name = Getattr(c, "sym:name");

        if (!first) { Printf(f_wrappers, "\n   "); } else { first = 0; }
        Printf(f_wrappers, "[%s %s]", slot_name, lisp_type);

        Append(entries, NewStringf("%s-%s", name, slot_name));
        Delete(lisp_type);
      }
    }
    Printf(f_wrappers, ")");
    write_wrapper_options(f_wrappers, Getattr(n, "feature:struct-options"),
                          tyname, Getattr(n, "name"));
    Printf(f_wrappers, ")\n\n");

    add_known_type(tyname, "struct");
    return SWIG_OK;
  }
  else if (!Strcmp(kind, "union")) {
    String *expr = stringOfUnion(n, 2);
    Printf(f_wrappers, "(define %s\n", tyname);
    Printf(f_wrappers, "  %s)\n\n", expr);

    add_known_type(tyname, "union");
    return SWIG_OK;
  }
  else {
    Printf(stderr, "Don't know how to deal with %s kind of class yet.\n", kind);
    Printf(stderr, " (name: %s)\n", name);
    SWIG_exit(EXIT_FAILURE);
    return 0; // unreachable (FIXME?)
  }
}


String *RACKET::stringOfFunction(Node *n, ParmList *pl, String *restype, int indent) {
  String *out = NewString("");
  Printf(out, "(_fun ");
  Parm *p = pl;

  while (p) {
    String *argname = Getattr(p, "name");
    String *tm = Getattr(p, "tmap:in");
    if (tm && !Strncmp(tm, "=", strlen("="))) {
      String *args = adjust_param_tm(tm, (indent > 0) ? indent + 6 - 2 : indent);
      Printf(out, "%s", args);
      Delete(args);
      p = Getattr(p, "tmap:in:next");
    } else {
      String *ffitype = get_ffi_type(n, Getattr(p, "type"));
      if (argname) {
        Printf(out, "[%s : %s]", argname, ffitype);
      } else {
        Printf(out, "%s", ffitype);
      }
      p = nextSibling(p);
    }
    writeIndent(out, indent, 6);
  }
  Printf(out, "-> %s)", restype);
  return out;
}

String *RACKET::adjust_param_tm(String *tmin, int indent) {
  char *s = Char(tmin) + strlen("=");
  while (isspace(*s) || (*s == '\n')) { s++; } // FIXME: is isspace('\n') true?
  String *tm = NewString(s);
  Chop(tm);
  if (indent >= 0) {
    String *indentation = NewString("\n");
    while (indent--) { Printf(indentation, " "); }
    Replaceall(tm, "\n", indentation);
  }
  return tm;
}

String *RACKET::stringOfUnion(Node *n, int indent) {
  String *out = NewString("");

  Printf(out, "(_union ");

  int first = 1;

  for (Node *c = firstChild(n); c; c = nextSibling(c)) {
    if (Strcmp(nodeType(c), "cdecl")) {
      Printf(stderr, "Structure %s has a slot that we can't deal with.\n", Getattr(n, "name"));
      Printf(stderr, "nodeType: %s, name: %s, type: %s\n",
             nodeType(c), Getattr(c, "name"), Getattr(c, "type"));
      SWIG_exit(EXIT_FAILURE);
    }

    String *temp = Copy(Getattr(c, "decl"));
    if (temp) {
      Append(temp, Getattr(c, "type"));	//appending type to the end, otherwise wrong type
      String *lisp_type = get_ffi_type(n, temp);
      Delete(temp);

      // String *slot_name = Getattr(c, "sym:name");
      if (!first) { writeIndent(out, indent, strlen("(_union ") + 2); } else { first = 0; }
      Printf(out, "%s", lisp_type);

      Delete(lisp_type);
    }
  }

  Printf(out, ")");

  return out;
}

void RACKET::writeIndent(String *out, int indent, int moreindent) {
  if (indent >= 0) {
    Printf(out, "\n");
    for (int i = 0; i < indent + moreindent; ++i) {
      Printf(out, " ");
    }
  } else {
    Printf(out, " ");
  }
}

/* utilities */
/* returns new string w/ parens stripped */
String *RACKET::strip_parens(String *string) {
  char *s = Char(string), *p;
  int len = Len(string);
  String *res;

  if (len == 0 || s[0] != '(' || s[len - 1] != ')') {
    return NewString(string);
  }

  p = (char *) malloc(len - 2 + 1);
  if (!p) {
    Printf(stderr, "Malloc failed\n");
    SWIG_exit(EXIT_FAILURE);
  }

  strncpy(p, s + 1, len - 1);
  p[len - 2] = 0;		/* null terminate */

  res = NewString(p);
  free(p);

  return res;
}

String *RACKET::convert_literal(String *num_param, String *type) {
  String *num = strip_parens(num_param), *res;
  char *s = Char(num);

  if (!Strcmp(type, "double")) {
    return num;
  }

  if (SwigType_type(type) == T_CHAR) {
    /* Use Racket syntax for character literals */
    return NewStringf("#\\%s", num_param);
  } else if (SwigType_type(type) == T_STRING) {
    /* Use Racket syntax for string literals */
    return NewStringf("\"%s\"", num_param);
  }

  if (Len(num) < 2 || s[0] != '0') {
    /* decimal */
    return num;
  } else {
    /* octal or hex */
    res = NewStringf("#%c%s", s[1] == 'x' ? 'x' : 'o', s + 2);
    Delete(num);
    return res;
  }
}

String *RACKET::get_mapped_type(Node *n, SwigType *ty) {
  Node *node = NewHash();
  Setattr(node, "type", ty);
  Setfile(node, Getfile(n));
  Setline(node, Getline(n));
  const String *tm = Swig_typemap_lookup("in", node, "", 0);
  Delete(node);
  return tm ? NewString(tm) : NULL;
}


String *RACKET::get_ffi_type(Node *n, SwigType *ty0) {
  String *result;
  SwigType *ty = Copy(ty0);     // private copy for mutation; delete at end

  if ((result = get_mapped_type(n, ty))) {
    ;
  }
  else if (SwigType_isqualifier(ty)) {
    SwigType_del_qualifier(ty);
    result = get_ffi_type(n, ty);
  }
  else if (SwigType_isarray(ty)) {
    String *array_dim = SwigType_array_getdim(ty, 0);
    if (!Strcmp(array_dim, "")) {	//dimension less array convert to pointer
      Delete(array_dim);
      SwigType_del_array(ty);
      SwigType_add_pointer(ty);
      result = get_ffi_type(n, ty);
    } else if (SwigType_array_ndim(ty) == 1) {
      SwigType_pop_arrays(ty);
      String *innertype = get_ffi_type(n, ty);
      result = NewStringf("(_array %s %s)", innertype, array_dim);
      Delete(array_dim);
      Delete(innertype);
    } else {
      String *innertype = get_ffi_type(n, ty);
      result = NewStringf("(_array %s (FIXME #| %s |#))", innertype, array_dim);
      Delete(innertype);
    }
  }
  else if (SwigType_isfunction(ty)) {
    // return NewStringf("_fpointer");
    SwigType *fn = SwigType_pop_function(ty);
    ParmList *pl = SwigType_function_parms(fn, n);
    String *restype = get_ffi_type(n, ty);
    String *expr = stringOfFunction(n, pl, restype, -1);
    Delete(fn);
    Delete(restype);
    return expr;
  }
  else if (SwigType_ispointer(ty)) {
    String *inner;
    SwigType_del_pointer(ty);
    if (SwigType_isconst(ty)) {
      SwigType_del_qualifier(ty);
    }

    if ((inner = get_mapped_type(n, ty))) {
      if (inner && is_known_struct_type(inner)) {
        result = NewStringf("%s-pointer/null", inner);
      } else {
        result = NewStringf("(_pointer-to %s)", inner);
      }
    }
    else if (SwigType_isfunction(ty)) {
      result = get_ffi_type(n, ty);
    }
    else {
      String *inner = get_ffi_type(n, ty);
      if (is_known_struct_type(inner)) {
        result = NewStringf("%s-pointer/null", inner);
      } else {
        result = NewStringf("(_pointer-to %s)", inner);
      }
      Delete(inner);
    }
  }
  else if (SwigType_issimple(ty)) {
    String *str = SwigType_str(ty, 0);
    int offset;

    if (!Strncmp(str, "struct ", strlen("struct "))) {
      offset = strlen("struct ");
      Setattr(used_structs, NewString(Char(str) + offset), "1");
    } else if (!Strncmp(str, "class ", strlen("class "))) {
      offset = strlen("class ");
    } else if (!Strncmp(str, "union ", strlen("union "))) {
      offset = strlen("union ");
    } else {
      offset = 0;
    }
    result = NewStringf("_%s", Char(str) + offset);
  }
  else {
    result = NewStringf("(_FIXME #| %s |#)", SwigType_str(ty, 0));
  }

  Delete(ty);
  return result;
}


extern "C" Language *swig_racket(void) {
  return new RACKET();
}
