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
#include <stdint.h>
#include "swigmod.h"

/* TODO:
 * - add variables to "rktin" typemap rhss, like $ffitype ?
 * - better, more consistent C->Racket expression handling
 *   - detect #define right-hand sides that are not legal Racket
 *     eg, "#define X (Y|0x01)"
 *   - array dimensions
 * - allow customizing provided names?
 *   or put wrappers in submodule, let interface file re-export, add contracts, etc?
 * - fix memory leaks
 */

static const char *usage = "\
Racket Options (available with -racket)\n\
     -extern-all       - Create Racket definitions for all the functions and\n\
                         global variables otherwise only definitions for\n\
                         externed functions and variables are created.\n\
     -emit-c-file      - Emit a C wrapper file.\n\
";

struct member_ctx {
  List *members;
  struct member_ctx *prev;
};

enum declared_t { undeclared, forward_declared, declared };

class RACKET:public Language {
public:
  int emit_c_file;
  int emit_define_foreign;
  File *f_cw;
  File *f_cbegin;
  File *f_cruntime;
  File *f_cheader;
  File *f_cwrappers;
  File *f_cinit;

  File *f_rkt;
  File *f_rktbegin;
  File *f_rkthead;
  File *f_rktwrap;
  File *f_rktinit;

  String *module;
  virtual void main(int argc, char *argv[]);
  virtual int top(Node *n);
  virtual int functionWrapper(Node *n);
  virtual int variableWrapper(Node *n);
  virtual int constantWrapper(Node *n);
  virtual int classDeclaration(Node *n);
  virtual int classforwardDeclaration(Node *n);
  virtual int enumDeclaration(Node *n);
  virtual int enumforwardDeclaration(Node *n);
  virtual int typedefHandler(Node *n);
  virtual int membervariableHandler(Node *);
  virtual int memberconstantHandler(Node *);
  virtual int memberfunctionHandler(Node *);
  virtual int constructorDeclaration(Node *);
  virtual int destructorDeclaration(Node *);
  List *entries;
private:
  String *get_ffi_type(Node *n, SwigType *ty);
  String *get_mapped_type(Node *n, SwigType *ty);
  void set_known_type(String *type, const char *prefix, String *ffitype, String *ptrtype);
  void add_known_pointer_type(String *type, String *ptrtype);
  String *get_known_pointer_type(String *type);
  enum declared_t get_type_declared(String *type, const char *prefix);
  void declare_forward_type(String *name, int isstruct);
  void write_function_params(File *out, Node *n, ParmList *pl, int indent, List *argouts);
  String *stringOfUnion(Node *n, int indent);
  int extern_all_flag;

  Hash *known_types;         // eg "struct point_st" -> "_point_st" or "_FWD-point_st"
  Hash *known_pointer_types; // eg "_point_st" -> "_point_st-pointer/null"

  Hash *used_structs;
  Hash *defined_structs;
  Hash *used_enums;
  Hash *defined_enums;
  struct member_ctx *mctx;
};

static void write_block(File *out, String *s, int indent, int subs, ...);
static String *adjust_block(String *tm, int indent);
static void write_wrapper_options(File *out, String *opts, int indent, String *ffiname, String *cname);
static String *convert_literal(String *num_param, String *type);
static String *convert_numeric_expr(char *s0);
static String *strip_parens(String *string);

static void write_function_prefix(File *out, String *prefix, int indent);
static void writeIndent(String *out, int indent, int moreindent);

void RACKET::main(int argc, char *argv[]) {
  int i;

  Preprocessor_define("SWIGRACKET 1", 0);
  SWIG_library_directory("racket");
  SWIG_config_file("racket.swg");
  extern_all_flag = 0;
  emit_c_file = 0;
  emit_define_foreign = 0;
  known_types = NewHash();
  known_pointer_types = NewHash();
  used_structs = NewHash();
  defined_structs = NewHash();
  used_enums = NewHash();
  defined_enums = NewHash();
  mctx = NULL;

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-help")) {
      Printf(stdout, "%s\n", usage);
    } else if ((Strcmp(argv[i], "-extern-all") == 0)) {
      extern_all_flag = 1;
      Swig_mark_arg(i);
    } else if ((Strcmp(argv[i], "-emit-c-file") == 0)) {
      emit_c_file = 1;
      Swig_mark_arg(i);
    } else if ((Strcmp(argv[i], "-emit-define-foreign") == 0)) {
      emit_define_foreign = 1;
      Swig_mark_arg(i);
    }
  }
}

enum declared_t RACKET::get_type_declared(String *type, const char *prefix = NULL) {
  if (prefix) { type = NewStringf("%s %s", prefix, type); }
  String *result = Getattr(known_types, type);
  if (prefix) { Delete(type); }
  if (!result) {
    return undeclared;
  } else if (!Strncmp(result, "_FWD-", strlen("_FWD-"))) {
    return forward_declared;
  } else {
    return declared;
  }
}

void RACKET::set_known_type(String *type, const char *prefix, String *ffitype, String *ptrtype) {
  // Maps both "type" and "prefix type" (if present), because of the
  // way Swig handles typedef to named struct/enum/union.
  // Printf(stderr, "Adding type: %s = %s\n", type, ffitype);
  Setattr(known_types, type, ffitype);
  if (ptrtype) {
    add_known_pointer_type(ffitype, ptrtype);
  }
  if (prefix) {
    type = NewStringf("%s %s", prefix, type);
    Setattr(known_types, type, ffitype);
  }
}

void RACKET::add_known_pointer_type(String *type, String *ptrtype) {
  // Printf(stderr, "Adding known pointer type: %s ptr = %s\n", type, ptrtype);
  Setattr(known_pointer_types, type, ptrtype);
}

String *RACKET::get_known_pointer_type(String *type) {
  return Getattr(known_pointer_types, type);
}

int RACKET::top(Node *n) {

  module = Getattr(n, "name");
  String *rkt_filename, *c_filename;
  entries = NewList();

  c_filename = Getattr(n, "outfile");
  if (emit_c_file) {
    f_cw = NewFile(c_filename, "w+", SWIG_output_files());
    if (!f_cw) {
      FileErrorDisplay(c_filename);
      SWIG_exit(EXIT_FAILURE);
    }
  } else {
    f_cw = NULL;
  }

  rkt_filename = NewStringf("%s%s.rkt", SWIG_output_directory(), module);
  f_rkt = NewFile(rkt_filename, "w+", SWIG_output_files());
  if (!f_rkt) {
    FileErrorDisplay(rkt_filename);
    SWIG_exit(EXIT_FAILURE);
  }

  {
    f_cbegin = NewString("");
    f_cruntime = NewString("");
    f_cheader = NewString("");
    f_cwrappers = NewString("");
    f_cinit = NewString("");

    // Register file targets with the SWIG file handler
    Swig_register_filebyname("begin", f_cbegin);
    Swig_register_filebyname("runtime", f_cruntime);
    Swig_register_filebyname("header", f_cheader);
    Swig_register_filebyname("wrapper", f_cwrappers);
    Swig_register_filebyname("init", f_cinit);
    Swig_banner_target_lang(f_cbegin, "//");
  }

  {
    f_rktbegin = NewString("");
    f_rkthead = NewString("");
    f_rktwrap = NewString("");
    f_rktinit = NewString("");

    // Register file targets with the SWIG file handler
    Swig_register_filebyname("rktbegin", f_rktbegin);
    Swig_register_filebyname("rktheader", f_rkthead);
    Swig_register_filebyname("rktwrapper", f_rktwrap);
    Swig_register_filebyname("rktinit", f_rktinit);
    Swig_banner_target_lang(f_rktbegin, ";;");
  }

  Language::top(n);

  if (emit_define_foreign) {
    Printf(f_rkthead, "(define foreign-lib (ffi-lib \"%s.so\"))\n", module);
    Printf(f_rkthead, "(define-ffi-definer define-foreign foreign-lib)\n\n");
  }

  if (1) {
    Dump(f_rktbegin, f_rkt);  Delete(f_rktbegin); f_rktbegin = NULL;
    Dump(f_rkthead, f_rkt);   Delete(f_rkthead);  f_rkthead = NULL;
    Dump(f_rktwrap, f_rkt);   Delete(f_rktwrap);  f_rktwrap = NULL;
    Dump(f_rktinit, f_rkt);   Delete(f_rktinit);  f_rktinit = NULL;
    Delete(f_rkt);            f_rkt = NULL;
  }

  if (emit_c_file) {
    Dump(f_cbegin, f_cw);     Delete(f_cbegin);      f_cbegin = NULL;
    Dump(f_cruntime, f_cw);   Delete(f_cruntime);    f_cruntime = NULL;
    Dump(f_cheader, f_cw);    Delete(f_cheader);     f_cheader = NULL;
    Dump(f_cwrappers, f_cw);  Delete(f_cwrappers);   f_cwrappers = NULL;
    Dump(f_cinit, f_cw);      Delete(f_cinit);       f_cinit = NULL;
    Delete(f_cw);             f_cw = NULL;
  }

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
  Swig_typemap_attach_parms("rktin", pl, 0);
  Swig_typemap_attach_parms("rktargout", pl, 0);

  String *restype = get_ffi_type(n, Getattr(n, "type"));
  List *argouts = NewList();
  String *result_expr = Getattr(n, "feature:fun-result");

  Printf(f_rktwrap, "(define-foreign %s\n", func_name);
  Printf(f_rktwrap, "  (_fun ");
  write_function_prefix(f_rktwrap, Getattr(n, "feature:fun-prefix"), 2 + strlen("(_fun "));
  write_function_params(f_rktwrap, n, pl, 2 + strlen("(_fun "), argouts);
  if (result_expr) {
    Printf(f_rktwrap, "-> [result : %s]", restype);
    writeIndent(f_rktwrap, 2 + strlen("(_fun "), 0);
    Printf(f_rktwrap, "-> %s)", result_expr);
  } else if (Len(argouts)) {
    if (!Strcmp(restype, "_void")) {
      Printf(f_rktwrap, "-> _void", restype);
      writeIndent(f_rktwrap, 2 + strlen("(_fun "), 0);
      Printf(f_rktwrap, "-> (values");
    } else {
      Printf(f_rktwrap, "-> [result : %s]", restype);
      writeIndent(f_rktwrap, 2 + strlen("(_fun "), 0);
      Printf(f_rktwrap, "-> (values result");
    }
    for (Iterator iter = First(argouts); iter.item; iter = Next(iter)) {
      Printf(f_rktwrap, " %s", iter.item);
    }
    Printf(f_rktwrap, "))");
  } else {
    Printf(f_rktwrap, "-> %s)", restype);
  }
  if (Strcmp(func_name, cname)) {
    Printf(f_rktwrap, "\n  #:c-id %s", cname);
  }
  write_wrapper_options(f_rktwrap, Getattr(n, "feature:fun-options"), 2, func_name, cname);
  Printf(f_rktwrap, ")\n\n");
  Delete(restype);

  return SWIG_OK;
}

int RACKET::constantWrapper(Node *n) {
  String *name = Getattr(n, "sym:name");
  String *cname = Getattr(n, "name");
  String *type = Getattr(n, "type");

  if (SwigType_isfunctionpointer(type) && cname) {
    // Result of %constant or %callback declaration, etc
    Printf(f_rktwrap, "(define-foreign %s _fpointer #:c-id %s)\n\n", name, cname);
  } else {
    String *converted_value = convert_literal(Getattr(n, "value"), type);
    Printf(f_rktwrap, "(define %s %s)\n\n", name, converted_value);
    Delete(converted_value);
  }
  Append(entries, name);

  return SWIG_OK;
}

int RACKET::variableWrapper(Node *n) {
  String *storage = Getattr(n, "storage");
  if (!extern_all_flag && (!storage || (!Swig_storage_isextern(n) && !Swig_storage_isexternc(n))))
    return SWIG_OK;

  String *var_name = Getattr(n, "sym:name");
  String *cname = Getattr(n, "name");
  String *lisp_type = get_ffi_type(n, Getattr(n, "type"));

  if (1) {
    Printf(f_rktwrap, "(define %s\n", var_name);
    Printf(f_rktwrap, "  (c-variable foreign-lib '%s '%s %s", var_name, cname, lisp_type);
    int indent = strlen("  (c-variable ");
    if (GetFlag(n, "feature:immutable")) {
      writeIndent(f_rktwrap, indent, 0);
      Printf(f_rktwrap, "#:immutable? #t");
    }
    write_wrapper_options(f_rktwrap, Getattr(n, "feature:var-options"), indent, var_name, cname);
    Printf(f_rktwrap, "))\n\n");
  } else {
    Printf(f_rktwrap, "(define-foreign %s %s", var_name, lisp_type);
    if (Strcmp(var_name, cname)) {
      Printf(f_rktwrap, "\n  #:c-id %s", cname);
    }
    write_wrapper_options(f_rktwrap, Getattr(n, "feature:var-options"), 2, var_name, cname);
    Printf(f_rktwrap, ")\n\n");
  }

  Append(entries, var_name);

  Delete(lisp_type);
  return SWIG_OK;
}

int RACKET::typedefHandler(Node *n) {
  String *tdname = Getattr(n, "name");
  String *tdtype = NewStringf("_%s", tdname);
  SwigType *ty = Getattr(n, "type");

  if (get_type_declared(tdname) != undeclared) {
    // COND was (get_known_pointer_type(tdtype) || Getattr(known_other_types, tdtype))
    // That means tdtype is already declared as a struct/enum/union, eg
    //   typedef struct point_st Point;
    // So just skip the typedef.
    ;
  } else {
    String *ffitype = get_ffi_type(n, ty);
    String *ptrtype = get_known_pointer_type(ffitype);

    if (ptrtype) {
      Printf(f_rktwrap, "(define %s %s)\n", tdtype, ffitype);
      Printf(f_rktwrap, "(define %s* %s)\n", tdtype, ptrtype);
      Printf(f_rktwrap, "\n");
      set_known_type(tdname, NULL, tdtype, NewStringf("%s*", tdtype));
    } else {
      Printf(f_rktwrap, "(define %s %s)\n", tdtype, ffitype);
      set_known_type(tdname, NULL, tdtype, NULL);
    }
  }
  return Language::typedefHandler(n);
}

int RACKET::classforwardDeclaration(Node *n) {
  if (0) {
    String *name = Getattr(n, "sym:name");
    String *kind = Getattr(n, "kind");
    Printf(stderr, "forward declaration of %s :: %s\n", name, kind);
  }
  return SWIG_OK;
}

int RACKET::enumforwardDeclaration(Node *n) {
  if (0) {
    String *name = Getattr(n, "sym:name");
    Printf(stderr, "forward declaration of %s :: enum\n", name);
  }
  return SWIG_OK;
}

void RACKET::declare_forward_type(String *name, int isstruct) {
  Printf(f_rkthead, "(define _FWD-%s (begin _void #| _%s |#))\n", name, name);
  if (isstruct) {
    Printf(f_rkthead, "(define _FWD-%s-pointer/null (_cpointer/null '%s) #| _%s-cpointer/null |#)\n",
           name, name, name);
  }
  Printf(f_rkthead, "\n");
}

int RACKET::enumDeclaration(Node *n) {
  if (getCurrentClass() && (cplus_mode != PUBLIC))
    return SWIG_NOWRAP;

  String *name = Getattr(n, "sym:name");
  String *tyname = NewStringf("_%s", name);
  set_known_type(name, "enum", tyname, NULL);

  Printf(f_rktwrap, "(define %s\n", tyname);
  Printf(f_rktwrap, "  (_enum '(");

  int first = 1;

  for (Node *c = firstChild(n); c; c = nextSibling(c)) {
    String *slot_name = Getattr(c, "name");
    String *value = Getattr(c, "enumvalue");
    if (!first) { Printf(f_rktwrap, " "); } else { first = 0; }
    if (value && Strcmp(value, "")) {
      Printf(f_rktwrap, "%s = %s", slot_name, value);
    } else {
      Printf(f_rktwrap, "%s", slot_name);
    }
    Append(entries, slot_name);
    Delete(value);
  }

  Printf(f_rktwrap, ")))\n\n");
  return SWIG_OK;
}

// Includes structs
int RACKET::classDeclaration(Node *n) {
  String *name = Getattr(n, "sym:name");
  String *tyname = NewStringf("_%s", name);
  String *kind = Getattr(n, "kind");
  int result;

  struct member_ctx my_mctx = { NewList(), mctx };
  mctx = &my_mctx;

  result = Language::classDeclaration(n);
  mctx = my_mctx.prev;

  if (result != SWIG_OK) return result;

  if (!Strcmp(kind, "struct")) {
    set_known_type(name, "struct", tyname, NewStringf("%s-pointer/null", name));

    Append(entries, NewStringf("make-%s", name));

    Printf(f_rktwrap, "(define-cstruct %s\n", tyname);
    Printf(f_rktwrap, "  (");

    int first = 1;

    for (Iterator iter = First(my_mctx.members); iter.item; iter = Next(iter)) {
      Node *c = iter.item;
      if (!Strcmp(nodeType(c), "cdecl")) {
        String *temp = Copy(Getattr(c, "decl"));
        if (temp) {
          Append(temp, Getattr(c, "type"));	//appending type to the end, otherwise wrong type
          String *lisp_type = get_ffi_type(n, temp);
          Delete(temp);

          String *slot_name = Getattr(c, "sym:name");

          if (!first) { writeIndent(f_rktwrap, 3, 0); } else { first = 0; }
          Printf(f_rktwrap, "[%s %s]", slot_name, lisp_type);

          Append(entries, NewStringf("%s-%s", name, slot_name));
          Delete(lisp_type);
        }
      } else {
        Printf(stderr, "Structure %s has a slot that we can't deal with.\n", name);
        Printf(stderr, "nodeType: %s, name: %s, type: %s\n",
               nodeType(c), Getattr(c, "name"), Getattr(c, "type"));
        SWIG_exit(EXIT_FAILURE);
      }
    }
    Printf(f_rktwrap, ")");
    write_wrapper_options(f_rktwrap, Getattr(n, "feature:struct-options"), 2,
                          tyname, Getattr(n, "name"));
    Printf(f_rktwrap, ")\n\n");

    add_known_pointer_type(tyname, NewStringf("%s-pointer/null", tyname));
    return SWIG_OK;
  }
  else if (!Strcmp(kind, "union")) {
    Printf(f_rktwrap, "(define %s\n", tyname);
    Printf(f_rktwrap, "  (_union ");

    int first = 1;

    for (Iterator iter = First(my_mctx.members); iter.item; iter = Next(iter)) {
      Node *c = iter.item;
      int indent = strlen("  (_union ");
      if (!Strcmp(nodeType(c), "cdecl")) {
        String *temp = Copy(Getattr(c, "decl"));
        if (temp) {
          Append(temp, Getattr(c, "type"));	//appending type to the end, otherwise wrong type
          String *lisp_type = get_ffi_type(n, temp);
          Delete(temp);

          if (!first) { writeIndent(f_rktwrap, indent, 0); } else { first = 0; }
          Printf(f_rktwrap, "%s", lisp_type);

          Delete(lisp_type);
        }
      } else {
        Printf(stderr, "Union %s has a slot that we can't deal with.\n", Getattr(n, "name"));
        Printf(stderr, "nodeType: %s, name: %s, type: %s\n",
               nodeType(c), Getattr(c, "name"), Getattr(c, "type"));
        SWIG_exit(EXIT_FAILURE);
      }
    }
    Printf(f_rktwrap, "))\n\n");

    set_known_type(name, "union", tyname, NULL);
    return SWIG_OK;
  }
  else {
    Printf(stderr, "Don't know how to deal with %s kind of class yet.\n", kind);
    Printf(stderr, " (name: %s)\n", name);
    SWIG_exit(EXIT_FAILURE);
    return 0; // unreachable (FIXME?)
  }
}

int RACKET::membervariableHandler(Node *n) {
  // Printf(stderr, "membervariableHandler %s\n", Getattr(n, "name"));
  if (mctx) {
    Append(mctx->members, n);
  }
  if (1) {
    return SWIG_OK;
  } else {
    // This declares getter and setter functions for the members.
    return Language::membervariableHandler(n);
  }
}

int RACKET::memberconstantHandler(Node *) {
  // Printf(stderr, "memberconstantHandler %s\n", Getattr(n, "name"));
  // return Language::memberconstantHandler(n);
  return SWIG_NOWRAP;
}

int RACKET::memberfunctionHandler(Node *) {
  // Printf(stderr, "memberfunctionHandler %s\n", Getattr(n, "name"));
  // return Language::memberfunctionHandler(n);
  return SWIG_NOWRAP;
}

int RACKET::constructorDeclaration(Node *) {
  // Printf(stderr, "constructorDeclaration %s\n", Getattr(n, "name"));
  return SWIG_NOWRAP;
}
int RACKET::destructorDeclaration(Node *) {
  // Printf(stderr, "destroctorDeclaration %s\n", Getattr(n, "name"));
  return SWIG_NOWRAP;
}


// ----------------------------------------

void RACKET::write_function_params(File *out, Node *n, ParmList *pl, int indent, List *argouts) {
  Parm *p = pl;
  String *ao_tm;
  while (p) {
    String *argname = Getattr(p, "name");
    String *tm = Getattr(p, "tmap:rktin");
    if ((ao_tm = Getattr(p, "tmap:rktargout"))) { Append(argouts, ao_tm); }
    if (tm) {
      String *args = adjust_block(tm, indent);
      Printf(out, "%s", args);
      Delete(args);
      p = Getattr(p, "tmap:rktin:next");
    } else {
      String *ffitype = get_ffi_type(n, Getattr(p, "type"));
      if (argname) {
        Printf(out, "[%s : %s]", argname, ffitype);
      } else {
        Printf(out, "%s", ffitype);
      }
      p = nextSibling(p);
    }
    writeIndent(out, indent, 0);
  }
}

String *RACKET::get_mapped_type(Node *n, SwigType *ty) {
  Node *node = NewHash();
  Setattr(node, "type", ty);
  Setfile(node, Getfile(n));
  Setline(node, Getline(n));
  const String *tm = Swig_typemap_lookup("rktffi", node, "", 0);
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
    // Unlike function decl, don't use "rktin", "rktargout" typemaps.
    SwigType *fn = SwigType_pop_function(ty);
    ParmList *pl = SwigType_function_parms(fn, n);
    String *restype = get_ffi_type(n, ty);
    String *out = NewString("");
    Printf(out, "(_fun ");
    write_function_params(out, n, pl, -1, NULL);
    Printf(out, "-> %s)", restype);
    Delete(fn);
    Delete(restype);
    return out;
  }
  else if (SwigType_ispointer(ty)) {
    String *inner;
    String *innerptrtype;
    SwigType_del_pointer(ty);
    while (SwigType_isqualifier(ty)) {
      SwigType_del_qualifier(ty);
    }

    if ((inner = get_mapped_type(n, ty))) {
      if (inner && (innerptrtype = get_known_pointer_type(inner))) {
        result = Copy(innerptrtype);
      } else {
        result = NewStringf("(_pointer-to %s)", inner);
      }
    }
    else if (SwigType_isfunction(ty)) {
      result = get_ffi_type(n, ty);
    }
    else {
      String *inner = get_ffi_type(n, ty);
      if ((innerptrtype = get_known_pointer_type(inner))) {
        result = Copy(innerptrtype);
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
    } else if (!Strncmp(str, "class ", strlen("class "))) {
      offset = strlen("class ");
    } else if (!Strncmp(str, "union ", strlen("union "))) {
      offset = strlen("union ");
    } else if (!Strncmp(str, "enum ", strlen("enum "))) {
      offset = strlen("enum ");
    } else {
      offset = 0;
    }
    String *name = NewString(Char(str) + offset);

    switch (get_type_declared(str)) {
    case undeclared:
      declare_forward_type(name, !Strncmp(str, "struct ", strlen("struct ")));
      result = NewStringf("_FWD-%s", name);
      break;
    case forward_declared:
      result = NewStringf("_FWD-%s", name);
      break;
    case declared:
      result = NewStringf("_%s", name);
      break;
    }
  }
  else {
    result = NewStringf("(_FIXME #| %s |#)", SwigType_str(ty, 0));
  }

  Delete(ty);
  return result;
}

// ============================================================

void write_function_prefix(File *out, String *prefix, int indent) {
  if (prefix && Strcmp(prefix, "")) {
    String *adjprefix = adjust_block(prefix, indent);
    Printf(out, "%s", adjprefix);
    writeIndent(out, indent, 0);
    Delete(adjprefix);
  }
}

void write_wrapper_options(File *out, String *s, int indent, String *ffiname, String *cname) {
  if (s && Strcmp(s, "")) {
    writeIndent(out, indent, 0);
    write_block(out, s, indent, 2, "$wrapname", ffiname, "$name", cname);
  }
}

void write_block(File *out, String *s, int indent, int subs, ...) {
  va_list args;
  va_start(args, subs);
  String *cp = adjust_block(s, indent);
  while (subs--) {
    const char *name = va_arg(args, const char*);
    String *value = va_arg(args, String *);
    Replaceall(cp, name, value);
  }
  va_end(args);
  Printf(out, "%s", cp);
  Delete(cp);
}

String *adjust_block(String *tmin, int indent) {
  char *s = Char(tmin);
  int len = Len(tmin);
  while (isspace(s[0]) || (s[0] == '\n')) { s++; len--; } // FIXME: is isspace('\n') true?
  while (isspace(s[len - 1]) || (s[len - 1] == '\n')) { len--; }
  String *tm = NewString("");
  Write(tm, s, len);
  if (indent >= 0) {
    String *indentation = NewString("\n");
    indent = indent - 2; // expected starting indentation
    while (indent--) { Printf(indentation, " "); }
    Replaceall(tm, "\n", indentation);
  }
  return tm;
}

void writeIndent(String *out, int indent, int moreindent) {
  if (indent >= 0) {
    Printf(out, "\n");
    for (int i = 0; i < indent + moreindent; ++i) {
      Printf(out, " ");
    }
  } else {
    Printf(out, " ");
  }
}

String *strip_parens(String *string) {
  char *s = Char(string);
  int len = Len(string);
  if (len == 0 || s[0] != '(' || s[len - 1] != ')') {
    return NewString(string);
  } else {
    String *res = NewString("");
    Write(res, s + 1, len - 2);
    return res;
  }
}

String *convert_literal(String *num_param, String *type) {
  String *num = strip_parens(num_param), *res;
  char *s = Char(num);

  if (SwigType_type(type) == T_CHAR) {
    // Use Racket syntax for character literals
    if ((Len(num) == 1) && (isgraph(s[0]))) {
      res = NewStringf("(char->integer #\\%s)", num);
    } else if (!Strcmp(num, " ")) {
      res = NewString("(char->integer #\\space)");
    } else if (!Strcmp(num, "\\n")) {
      res = NewString("(char->integer #\\newline)");
    } else if (!Strcmp(num, "\\r")) {
      res = NewString("(char->integer #\\return)");
    } else if (!Strcmp(num, "\\t")) {
      res = NewString("(char->integer #\\tab)");
    } else {
      res = NewStringf("(bytes-ref #\"%s\" 0)", num);
    }
  } else if (SwigType_type(type) == T_STRING) {
    // Use Racket syntax for string literals
    // FIXME: needs escaping!
    res = NewStringf("\"%s\"", num_param);
  } else {
    res = convert_numeric_expr(s);
  }
  Delete(num);
  // Printf(stderr, "constant = '%s', type = %s\n", num_param, type);
  // Printf(stderr, "  -> '%s'\n", res);
  return res;
}

String *convert_numeric_expr(char *s0) {
  String *out = NewString("");
  char *s = s0;
  char *end = s + strlen(s);
  int read = 0;
  char op = 0;

  intptr_t ivalue;
  double dvalue;
  char svalue[10];

 READ_NUM:
  sscanf(s, " %n", &read);
  s = s + read; read = 0;
  if ((sscanf(s, "%ji%n", &ivalue, &read) == 1) && (s[read] !='.')) {
    goto GOT_INTEGER;
  } else if (sscanf(s, "%lf%n", &dvalue, &read) == 1) {
    // Printf(out, "%lf ", dvalue);
    Write(out, s, read); Printf(out, " ");
    s = s + read;
    goto READ_OP;
  } else {
    goto ERROR;
  }

 GOT_INTEGER:
  if (read == 1 || s[0] != '0') {
    // decimal
    Write(out, s, read);
  } else if (s[1] == 'x' || s[1] == 'X') {
    // hexadecimal
    Printf(out, "#x");
    Write(out, s + 2, read - 2);
  } else {
    // octal
    Printf(out, "#o");
    Write(out, s + 1, read - 1);
  }
  Printf(out, " ");
  s = s + read;
  goto READ_OP;

 READ_OP:
  sscanf(s, " %n", &read);
  s = s + read; read = 0;
  if (s == end) {
    goto SUCCESS;
  } else if (sscanf(s, "%1[|]%n", &svalue[0], &read)) {
    if (!op || op == '|') {
      op = '|';
      s = s + read;
      goto READ_NUM;
    } else {
      goto ERROR;
    }
  } else if (sscanf(s, "%1[+]%n", &svalue[0], &read)) {
    if (!op || op == '+') {
      op = '+';
      s = s + read;
      goto READ_NUM;
    } else {
      goto ERROR;
    }
  } else {
    goto ERROR;
  }

 ERROR:
  Delete(out);
  return NewStringf("(FIXME #| %s |#)", s0);

 SUCCESS:
  Chop(out);
  if (op == 0) {
    return out;
  } else if (op == '|') {
    String *result = NewStringf("(bitwise-ior %s)", out);
    Delete(out);
    return result;
  } else if (op == '+') {
    String *result = NewStringf("(+ %s)", out);
    Delete(out);
    return result;
  } else {
    goto ERROR;
  }
}

// ============================================================

extern "C" Language *swig_racket(void) {
  return new RACKET();
}
