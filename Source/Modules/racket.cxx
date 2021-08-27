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

#include "swigmod.h"

static const char *usage = "\
Racket Options (available with -racket)\n\
     -extern-all       - Create Racket definitions for all the functions and\n\
                         global variables otherwise only definitions for\n\
                         externed functions and variables are created.\n\
";

class RACKET:public Language {
public:
  File *f_rkt;
  String *module;
  virtual void main(int argc, char *argv[]);
  virtual int top(Node *n);
  virtual int functionWrapper(Node *n);
  virtual int variableWrapper(Node *n);
  virtual int constantWrapper(Node *n);
  virtual int classDeclaration(Node *n);
  virtual int enumDeclaration(Node *n);
  virtual int typedefHandler(Node *n);
  List *entries;
private:
  String *get_ffi_type(Node *n, SwigType *ty);
  String *convert_literal(String *num_param, String *type);
  String *strip_parens(String *string);
  String *stringOfFunction(Node *n, ParmList *pl, String *restype, int indent);
  String *stringOfUnion(Node *n, int indent);
  void writeIndent(String *out, int indent);
  int extern_all_flag;
  int generate_typedef_flag;
  int is_function;
};

void RACKET::main(int argc, char *argv[]) {
  int i;

  Preprocessor_define("SWIGRACKET 1", 0);
  SWIG_library_directory("racket");
  SWIG_config_file("racket.swg");
  generate_typedef_flag = 0;
  extern_all_flag = 0;

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-help")) {
      Printf(stdout, "%s\n", usage);
    } else if ((Strcmp(argv[i], "-extern-all") == 0)) {
      extern_all_flag = 1;
      Swig_mark_arg(i);
    } else if ((Strcmp(argv[i], "-generate-typedef") == 0)) {
      generate_typedef_flag = 1;
      Swig_mark_arg(i);
    }
  }
}

int RACKET::top(Node *n) {

  File *f_null = NewString("");
  module = Getattr(n, "name");
  String *output_filename;
  entries = NewList();

  /* Get the output file name */
  String *outfile = Getattr(n, "outfile");

  if (!outfile) {
    Printf(stderr, "Unable to determine outfile\n");
    SWIG_exit(EXIT_FAILURE);
  }

  output_filename = NewStringf("%s%s.rkt", SWIG_output_directory(), module);

  f_rkt = NewFile(output_filename, "w+", SWIG_output_files());
  if (!f_rkt) {
    FileErrorDisplay(output_filename);
    SWIG_exit(EXIT_FAILURE);
  }

  Swig_register_filebyname("header", f_null);
  Swig_register_filebyname("begin", f_null);
  Swig_register_filebyname("runtime", f_null);
  Swig_register_filebyname("wrapper", f_null);

  String *header = NewString("");

  Swig_banner_target_lang(header, ";;");

  Printf(header, "#lang racket/base\n");
  Printf(header, "(require ffi/unsafe ffi/unsafe/define)\n");
  Printf(header, "(provide (protect-out (all-defined-out)))\n");
  Printf(header, "\n");
  Printf(header, "(define-ffi-definer define-foreign\n");
  Printf(header, "  FIXME)\n");
  Printf(header, "\n");

  Printf(f_rkt, "%s", header);

  Language::top(n);

  Delete(f_rkt);
  return SWIG_OK;
}


int RACKET::functionWrapper(Node *n) {
  is_function = 1;
  String *storage = Getattr(n, "storage");

  /*
  if (!extern_all_flag && (!storage || (!Swig_storage_isextern(n) && !Swig_storage_isexternc(n))))
    return SWIG_OK;
  */

  String *func_name = Getattr(n, "sym:name");

  Append(entries, func_name);

  ParmList *pl = Getattr(n, "parms");
  String *restype = get_ffi_type(n, Getattr(n, "type"));
  String *expr = stringOfFunction(n, pl, restype, 2);
  Printf(f_rkt, "(define-foreign %s\n", func_name);
  Printf(f_rkt, "  %s)\n\n", expr);
  Delete(expr);
  Delete(restype);

  return SWIG_OK;
}


int RACKET::constantWrapper(Node *n) {
  is_function = 0;
  String *type = Getattr(n, "type");
  String *converted_value = convert_literal(Getattr(n, "value"), type);
  String *name = Getattr(n, "sym:name");

  Printf(f_rkt, "(define %s %s)\n\n", name, converted_value);
  Append(entries, name);
  Delete(converted_value);

  return SWIG_OK;
}

int RACKET::variableWrapper(Node *n) {
  is_function = 0;
  String *storage = Getattr(n, "storage");
  if (!extern_all_flag && (!storage || (!Swig_storage_isextern(n) && !Swig_storage_isexternc(n))))
    return SWIG_OK;

  String *var_name = Getattr(n, "sym:name");
  String *lisp_type = get_ffi_type(n, Getattr(n, "type"));

  Printf(f_rkt, "(define-foreign %s %s)\n", var_name, lisp_type);
  // FIXME: use #:c-id for overridden name
  Append(entries, var_name);

  Delete(lisp_type);
  return SWIG_OK;
}

int RACKET::typedefHandler(Node *n) {
  if (generate_typedef_flag) {
    is_function = 0;
    Printf(f_rkt, "(define _%s\n", Getattr(n, "name"));
    Printf(f_rkt, "  %s)\n\n", get_ffi_type(n, Getattr(n, "type")));
  }
  return Language::typedefHandler(n);
}

int RACKET::enumDeclaration(Node *n) {
  if (getCurrentClass() && (cplus_mode != PUBLIC))
    return SWIG_NOWRAP;

  is_function = 0;
  String *name = Getattr(n, "sym:name");

  Printf(f_rkt, "(define _%s\n", name);
  Printf(f_rkt, "  (_enum '(");

  int first = 1;

  for (Node *c = firstChild(n); c; c = nextSibling(c)) {
    String *slot_name = Getattr(c, "name");
    String *value = Getattr(c, "enumvalue");
    if (!first) { Printf(f_rkt, " "); } else { first = 0; }
    if (value && !Strcmp(value, "")) {
      Printf(f_rkt, "%s = %s", slot_name, value);
    } else {
      Printf(f_rkt, "%s", slot_name);
    }
    Append(entries, slot_name);
    Delete(value);
  }

  Printf(f_rkt, ")))\n\n");
  return SWIG_OK;
}


// Includes structs
int RACKET::classDeclaration(Node *n) {
  is_function = 0;
  String *name = Getattr(n, "sym:name");
  String *kind = Getattr(n, "kind");

  if (!Strcmp(kind, "struct")) {
    Append(entries, NewStringf("make-%s", name));

    Printf(f_rkt, "(define-cstruct _%s\n", name);
    Printf(f_rkt, "  (");

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

        if (!first) { Printf(f_rkt, "\n   "); } else { first = 0; }
        Printf(f_rkt, "[%s %s]", slot_name, lisp_type);

        Append(entries, NewStringf("%s-%s", name, slot_name));
        Delete(lisp_type);
      }
    }

    Printf(f_rkt, "))\n\n");

    // FIXME!
    /* Add this structure to the known lisp types */
    //Printf(stdout, "Adding %s foreign type\n", name);
    //  add_defined_foreign_type(name);

    return SWIG_OK;
  }
  else if (!Strcmp(kind, "union")) {
    String *expr = stringOfUnion(n, 2);
    Printf(f_rkt, "(define _%s\n", name);
    Printf(f_rkt, "  %s)\n\n", expr);

    // FIXME! Add this structure to the known lisp types
    //Printf(stdout, "Adding %s foreign type\n", name);
    //  add_defined_foreign_type(name);

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
  int argnum = 0;

  Printf(out, "(_fun ");
  for (Parm *p = pl; p; p = nextSibling(p), argnum++) {
    String *argname = Getattr(p, "name");
    String *ffitype = get_ffi_type(n, Getattr(p, "type"));
    if (argname) {
      Printf(out, "[%s : %s]\n", argname, ffitype);
    } else {
      Printf(out, "%s\n", ffitype);
    }
    writeIndent(out, indent + 6);
  }
  Printf(out, "-> %s)", restype);
  return out;
}

String *RACKET::stringOfUnion(Node *n, int indent) {
  String *out = NewString("");

  Printf(out, "(_union");

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
      if (!first) { Printf(out, "\n  "); writeIndent(out, indent); } else { first = 0; }
      Printf(out, " %s", lisp_type);

      Delete(lisp_type);
    }
  }

  Printf(out, ")");

  return out;
}

void RACKET::writeIndent(String *out, int indent) {
  for (int i = 0; i < indent; ++i) {
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

String *RACKET::get_ffi_type(Node *n, SwigType *ty) {
  Node *node = NewHash();
  Setattr(node, "type", ty);
  Setfile(node, Getfile(n));
  Setline(node, Getline(n));
  const String *tm = Swig_typemap_lookup("in", node, "", 0);
  Delete(node);

  if (tm) {
    return NewString(tm);
  }
  else if (SwigType_ispointer(ty)) {
    SwigType *cp = Copy(ty);
    SwigType_del_pointer(cp);
    String *expr = get_ffi_type(n, cp);
    String *res;
    if (SwigType_isfunction(cp)) {
      res = expr;
    } else {
      res = NewStringf("(_ptr ?? %s)", expr);
      Delete(expr);
    }
    Delete(cp);
    return res;
  }
  /* else if (SwigType_ispointer(ty)) {
    SwigType *cp = Copy(ty);
    SwigType_del_pointer(cp);
    String *inner_type = get_ffi_type(n, cp);

    if (SwigType_isfunction(cp)) {
      return inner_type;
    }

    SwigType *base = SwigType_base(ty);
    String *base_name = SwigType_str(base, 0);

    String *str;
    if (!Strcmp(base_name, "int") || !Strcmp(base_name, "float") || !Strcmp(base_name, "short")
	|| !Strcmp(base_name, "double") || !Strcmp(base_name, "long") || !Strcmp(base_name, "char")) {

      str = NewStringf("(ffi:c-ptr %s)", inner_type);
    } else {
      str = NewStringf("(ffi:c-pointer %s)", inner_type);
    }
    Delete(base_name);
    Delete(base);
    Delete(cp);
    Delete(inner_type);
    return str;
  } */
  /* else if (SwigType_isarray(ty)) {
    SwigType *cp = Copy(ty);
    String *array_dim = SwigType_array_getdim(ty, 0);

    if (!Strcmp(array_dim, "")) {	//dimension less array convert to pointer
      Delete(array_dim);
      SwigType_del_array(cp);
      SwigType_add_pointer(cp);
      String *str = get_ffi_type(n, cp);
      Delete(cp);
      return str;
    } else {
      SwigType_pop_arrays(cp);
      String *inner_type = get_ffi_type(n, cp);
      Delete(cp);

      int ndim = SwigType_array_ndim(ty);
      String *dimension;
      if (ndim == 1) {
	dimension = array_dim;
      } else {
	dimension = array_dim;
	for (int i = 1; i < ndim; i++) {
	  array_dim = SwigType_array_getdim(ty, i);
	  Append(dimension, " ");
	  Append(dimension, array_dim);
	  Delete(array_dim);
	}
	String *temp = dimension;
	dimension = NewStringf("(%s)", dimension);
	Delete(temp);
      }
      String *str;
      if (is_function)
	str = NewStringf("(ffi:c-ptr (ffi:c-array %s %s))", inner_type, dimension);
      else
	str = NewStringf("(ffi:c-array %s %s)", inner_type, dimension);

      Delete(inner_type);
      Delete(dimension);
      return str;
    }
  } */
  else if (SwigType_isfunction(ty)) {
    // FIXME
    // return NewStringf("_fpointer");
    SwigType *cp = Copy(ty);
    SwigType *fn = SwigType_pop_function(cp);
    ParmList *pl = SwigType_function_parms(fn, n);
    String *restype = get_ffi_type(n, cp);
    String *expr = stringOfFunction(n, pl, restype, 8); // FIXME, indent
    Delete(fn);
    Delete(cp);
    Delete(restype);
    return expr;
  }
  /* else if (SwigType_isfunction(ty)) {
    SwigType *cp = Copy(ty);
    SwigType *fn = SwigType_pop_function(cp);
    String *args = NewString("");
    ParmList *pl = SwigType_function_parms(fn, n);
    if (ParmList_len(pl) != 0) {
      Printf(args, "(:arguments ");
    }
    int argnum = 0, first = 1;
    for (Parm *p = pl; p; p = nextSibling(p), argnum++) {
      String *argname = Getattr(p, "name");
      SwigType *argtype = Getattr(p, "type");
      String *ffitype = get_ffi_type(n, argtype);

      int tempargname = 0;

      if (!argname) {
	argname = NewStringf("arg%d", argnum);
	tempargname = 1;
      }
      if (!first) {
	Printf(args, "\n\t\t");
      }
      Printf(args, "(%s %s)", argname, ffitype);
      first = 0;
      Delete(ffitype);
      if (tempargname)
	Delete(argname);
    }
    if (ParmList_len(pl) != 0) {
      Printf(args, ")\n");	// finish arg list
    }
    String *ffitype = get_ffi_type(n, cp);
    String *str = NewStringf("(ffi:c-function %s \t\t\t\t(:return-type %s))", args, ffitype);
    Delete(fn);
    Delete(args);
    Delete(cp);
    Delete(ffitype);
    return str;
  } */
  else {
    String *str = SwigType_str(ty, 0);
    if (str) {
      char *st = Strstr(str, "struct");
      if (st) {
        st += 7;
        return NewString(st);
      }
      char *cl = Strstr(str, "class");
      if (cl) {
        cl += 6;
        return NewString(cl);
      }
    }
    return str;
  }
}

extern "C" Language *swig_racket(void) {
  return new RACKET();
}
