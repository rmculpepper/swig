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

// ============================================================
// DeclItems

class DeclItem {
public:
  String *str;  // Racket code to emit
  List *deps;   // list of { String* | NewVoid(DeclItem*) }
  int inserted;
  DeclItem *joinwith;

  DeclItem() {
    reset();
  }

  void reset() {
    this->str = NewString("");
    this->deps = NewList();
    this->inserted = 0;
    this->joinwith = NULL;
  }

  void addDep(DeclItem *dep) {
    if (dep) {
      DOH *p = NewVoid(dep, NULL); // FIXME: free
      Append(this->deps, p);
    }
  }

  void addString(String *s) {
    Append(this->deps, s);
  }

  void write(File *out) {
    (void)writek(out, NULL);
    Printf(out, "\n");
  }

  void *writek(File *out, void *prev) {
    if (!inserted) {
      inserted = 1;
      for (Iterator iter = First(deps); iter.item; iter = Next(iter)) {
        if (DohIsString(iter.item)) {
          if (prev != NULL) { Printf(out, "\n"); }
          Printf(out, "%s", iter.item);
          prev = iter.item;
        } else {
          DeclItem *item = (DeclItem*)Data(iter.item);
          prev = item->writek(out, prev);
        }
      }
      if (Len(str)) {
        if (prev != joinwith) { Printf(out, "\n"); }
        Printf(out, "%s", str);
        prev = this;
      }
    }
    return prev;
  }
};

// ============================================================
// Type Records

enum declared_t { undeclared, forward_declared, fully_declared };

class TypeRecord {
public:
  enum declared_t declared;
  String *ctype;
  const char *ckind;
  String *cname;
  String *ffitype;    // Racket expr for type
  String *ptrtype;    // Racket expr for ptr type, or NULL
  DeclItem *decl;     // Racket declaration of ffitype
  DeclItem *ptrdecl;  // Racket declaration of ptrtype (if not NULL)

  int isKnown() { return (declared != undeclared); }
  int isDefined() { return (declared == fully_declared); }
  int isStruct() { return !Strcmp(ckind, "struct"); }
  int isEnum() { return !Strcmp(ckind, "enum"); }
  int isUnion() { return !Strcmp(ckind, "union"); }

  void addIndyPtrType() {
    this->ptrtype = NewStringf("%s*", this->ffitype);
    this->ptrdecl = new DeclItem();
    this->ptrdecl->joinwith = this->decl;
  }

  void setStructDefined() {
    this->ptrtype = NewStringf("%s-pointer/null", this->ffitype);
    this->ptrdecl = this->decl; // same declaration defines both
  }

  TypeRecord(String *ctype) {
    char *str = Char(ctype);
    this->declared = undeclared;
    this->ctype = Copy(ctype);
    if (!Strncmp(str, "struct ", strlen("struct "))) {
      this->ckind = "struct";
      this->cname = NewString(str + strlen("struct "));
    } else if (!Strncmp(str, "class ", strlen("class "))) {
      this->ckind = "class";
      this->cname = NewString(str + strlen("class "));
    } else if (!Strncmp(str, "union ", strlen("union "))) {
      this->ckind = "union";
      this->cname = NewString(str + strlen("union "));
    } else if (!Strncmp(str, "enum ", strlen("enum "))) {
      this->ckind = "enum";
      this->cname = NewString(str + strlen("enum "));
    } else {
      this->ckind = "";
      this->cname = NewString(str);
    }
    this->ffitype = NewStringf("_%s", this->cname);
    if (!Strcmp(this->ckind, "struct")) {
      this->ptrtype = NewStringf("(_cpointer/null '%s)", this->cname);
    } else {
      this->ptrtype = NULL;
    }

    this->decl = new DeclItem();
    Printf(this->decl->str, ";; Incomplete or missing declaration for %s\n", this->ctype);
    if (this->isEnum()) {
      Printf(this->decl->str, "(define %s _fixint)\n", this->ffitype);
    } else {
      Printf(this->decl->str, "(define %s (_FIXME #| %s |#))\n", this->ffitype, this->ctype);
    }

    this->ptrdecl = NULL;
  }
};

// ============================================================

struct member_ctx {
  List *members;
  struct member_ctx *prev;
};

class RACKET:public Language {
public:
  int emit_c_file;
  int emit_define_foreign;
  int extern_all_flag;

  File *f_cw;
  File *f_cbegin;
  File *f_cruntime;
  File *f_cheader;
  File *f_cwrappers;
  File *f_cinit;

  File *f_rkt;
  File *f_rktbegin;
  File *f_rkthead;
  DeclItem *f_rkttypes;
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
private:
  TypeRecord *getTypeRecord(String *type, const String_or_char *prefix = NULL);
  void registerTypeRecord(String *ctype, TypeRecord *tr);
  TypeRecord *getRacketTypeRecord(String *ffitype);
  void declareForwardType(TypeRecord *tr);
  void writeFunParams(File *out, Node *n, ParmList *pl, int indent, List *argouts, DeclItem *client);
  String *getMappedType(Node *n, SwigType *ty);
  String *getRacketType(Node *n, SwigType *ty, DeclItem *di = NULL);
  void scanForNested(Node *n);

  Hash *type_records;        // eg "struct point_st" -> new TypeRecord(....)
  Hash *ffitype_records;     // eg "_point" -> new TypeRecord(....)
  List *fixup_list;          // eg, ["struct point_st", "enum color"]
  struct member_ctx *mctx;
};

static void write_block(File *out, String *s, int indent, int subs, ...);
static String *adjust_block(String *tm, int indent);
static void write_wrapper_options(File *out, String *opts, int indent, String *ffiname, String *cname);
static String *convert_literal(String *num_param, String *type);
static String *convert_numeric_expr(char *s0);
static String *strip_parens(String *string);

static void writeFunPrefix(File *out, String *prefix, int indent);
static void writeIndent(String *out, int indent, int moreindent);

enum define_types_t { define_none = 0, define_main = 1, define_ptr = 2, define_all = 3 };

static enum define_types_t getDefineTypesMode(Node *n);

// ============================================================
// Racket Language Module

void RACKET::main(int argc, char *argv[]) {
  int i;

  Preprocessor_define("SWIGRACKET 1", 0);
  SWIG_library_directory("racket");
  SWIG_config_file("racket.swg");
  extern_all_flag = 0;
  emit_c_file = 0;
  emit_define_foreign = 0;
  type_records = NewHash();
  ffitype_records = NewHash();
  fixup_list = NewList();
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

TypeRecord *RACKET::getTypeRecord(String *ctype, const String_or_char *prefix) {
  TypeRecord *tr;
  if (prefix) { ctype = NewStringf("%s %s", prefix, ctype); }
  DOH *p = Getattr(type_records, ctype);
  if (p) {
    tr = (TypeRecord*)Data(p);
  } else {
    tr = new TypeRecord(ctype);
    registerTypeRecord(tr->ctype, tr);
  }
  if (prefix) { Delete(ctype); }
  return tr;
}

void RACKET::registerTypeRecord(String *ctype, TypeRecord *tr) {
  DOH *p = NewVoid(tr, NULL); // FIXME: add free callback
  Setattr(type_records, ctype, p);
  Setattr(ffitype_records, tr->ffitype, p);
}

TypeRecord *RACKET::getRacketTypeRecord(String *ffitype) {
  DOH *p = Getattr(ffitype_records, ffitype);
  return p ? (TypeRecord*)Data(p) : NULL;
}

int RACKET::top(Node *n) {

  module = Getattr(n, "name");
  String *rkt_filename, *c_filename;

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
    f_rkttypes = new DeclItem();
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
  if (0) {
    Printf(f_rkthead, "(define (FIXME . args) 'FIXME)\n\n");
    Printf(f_rkthead, "(define (_FIXME . args) _void)\n\n");
  }

  if (1) {
    Dump(f_rktbegin, f_rkt);  Delete(f_rktbegin); f_rktbegin = NULL;
    Dump(f_rkthead,  f_rkt);  Delete(f_rkthead);  f_rkthead = NULL;

    Printf(f_rkt, ";; ----------------------------------------\n");
    Printf(f_rkt, ";; Types and Constants\n\n");
    f_rkttypes->write(f_rkt);

    Printf(f_rkt, ";; ----------------------------------------\n");
    Printf(f_rkt, ";; Wrappers\n\n");
    Dump(f_rktwrap,  f_rkt);  Delete(f_rktwrap);  f_rktwrap = NULL;

    if (Len(f_rktinit)) {
      Printf(f_rkt, ";; ----------------------------------------\n");
      Printf(f_rkt, ";; Initialization\n\n");
    }
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

  String *out = NewString("");
  String *func_name = Getattr(n, "sym:name");
  String *cname = Getattr(n, "name");
  // Printf(stderr, "## fun %s\n", cname);

  ParmList *pl = Getattr(n, "parms");
  Swig_typemap_attach_parms("rktin", pl, 0);
  Swig_typemap_attach_parms("rktargout", pl, 0);

  String *result_ctype = Getattr(n, "type");
  String *restype = getRacketType(n, result_ctype, NULL);
  List *argouts = NewList();
  String *result_expr = Getattr(n, "feature:fun-result");

  Printf(out, "(define-foreign %s\n", func_name);
  Printf(out, "  (_fun ");
  writeFunPrefix(out, Getattr(n, "feature:fun-prefix"), 2 + strlen("(_fun "));
  writeFunParams(out, n, pl, 2 + strlen("(_fun "), argouts, NULL);
  if (result_expr) {
    Printf(out, "-> [result : %s]", restype);
    writeIndent(out, 2 + strlen("(_fun "), 0);
    Printf(out, "-> %s)", result_expr);
  } else if (Len(argouts)) {
    if (!Strcmp(restype, "_void")) {
      Printf(out, "-> _void", restype);
      writeIndent(out, 2 + strlen("(_fun "), 0);
      Printf(out, "-> (values");
    } else {
      Printf(out, "-> [result : %s]", restype);
      writeIndent(out, 2 + strlen("(_fun "), 0);
      Printf(out, "-> (values result");
    }
    for (Iterator iter = First(argouts); iter.item; iter = Next(iter)) {
      Printf(out, " %s", iter.item);
    }
    Printf(out, "))");
  } else {
    Printf(out, "-> %s)", restype);
  }
  if (Strcmp(func_name, cname)) {
    Printf(out, "\n  #:c-id %s", cname);
  }
  if (GetFlag(n, "feature:new")) {
    String *free_fun = Swig_typemap_lookup("rktnewfree", n, result_ctype, NULL);
    if (free_fun) {
      Printf(out, "\n  #:wrap (allocator %s)", free_fun);
    } else {
      Printf(out, "\n  #:wrap (allocator free)");
    }
  } else if (GetFlag(n, "feature:del")) {
    Printf(out, "\n  #:wrap (deallocator)");
  }
  write_wrapper_options(out, Getattr(n, "feature:fun-options"), 2, func_name, cname);
  Printf(out, ")\n\n");
  Delete(restype);

  Printf(f_rktwrap, "%s", out);
  Delete(out);
  return SWIG_OK;
}

int RACKET::constantWrapper(Node *n) {
  String *name = Getattr(n, "sym:name");
  String *cname = Getattr(n, "name");
  String *type = Getattr(n, "type");
  // Printf(stderr, "## const %s\n", cname);

  if (SwigType_isfunctionpointer(type) && cname) {
    // Result of %constant or %callback declaration, etc
    Printf(f_rktwrap, "(define-foreign %s _fpointer #:c-id %s)\n\n", name, cname);
  } else {
    String *converted_value = convert_literal(Getattr(n, "value"), type);
    f_rkttypes->addString(NewStringf("(define %s %s)\n", name, converted_value));
    Delete(converted_value);
  }

  return SWIG_OK;
}

int RACKET::variableWrapper(Node *n) {
  String *storage = Getattr(n, "storage");
  if (!extern_all_flag && (!storage || (!Swig_storage_isextern(n) && !Swig_storage_isexternc(n))))
    return SWIG_OK;

  String *var_name = Getattr(n, "sym:name");
  String *cname = Getattr(n, "name");
  String *lisp_type = getRacketType(n, Getattr(n, "type"), NULL);
  // Printf(stderr, "## var %s\n", cname);

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

  Delete(lisp_type);
  return SWIG_OK;
}

/* Type declarations and typedefs
 
   "typedef struct foo_st {...} Foo;" 
   produces a class with
   - name = "foo_st"
   - kind = "struct"
   - sym:name = "Foo"
   - tdname = "Foo"
   and a cdecl with
   - name = "Foo"
   - kind = "typedef"
   - type = "struct foo_st"

   "struct bar_st {...};"
   produces a class with
   - name = "bar_st"
   - kind = "struct"
   - sym:name = "bar_st"

   typedef struct bar_st Bar;"
   produces a cdecl with
   - name = "Bar"
   - kind = "typedef"
   - sym:name = "Bar"
   - type = "struct bar_st"
*/

int RACKET::typedefHandler(Node *n) {
  String *tdname = Getattr(n, "name");
  // Printf(stderr, "## typedef %s\n", tdname);
  TypeRecord *tdtr = getTypeRecord(tdname);

  if (tdtr->isKnown()) {
    // For example, maybe tdname is already declared as a struct/enum/union:
    //   typedef struct point_st Point;
  } else {
    SwigType *ty = Getattr(n, "type");
    tdtr->decl->reset();
    String *rhsrtype = getRacketType(n, ty, tdtr->decl);
    TypeRecord *rhs = getRacketTypeRecord(rhsrtype);
    if (Strcmp(tdtr->ffitype, rhsrtype)) {
      Printf(tdtr->decl->str, "(define %s %s)\n", tdtr->ffitype, rhsrtype);
      if (1) {
        // FIXME: It is common to "typedef struct point_st Point" and then only
        // use "Point*", leaving "struct point_st" incomplete. That is, ptrdecl
        // is used, but decl is unused. Add option to omit main decl if unused.
        // BUT then wrappers need to trigger deps too :(
        f_rkttypes->addDep(tdtr->decl);
      }
    } else {
      // If same names, just omit to avoid (define _Type _Type).
    }

    if (rhs && rhs->ptrtype) {
      tdtr->addIndyPtrType();
      Printf(tdtr->ptrdecl->str, "(define %s %s)\n", tdtr->ptrtype, rhs->ptrtype);
      tdtr->ptrdecl->addDep(rhs->ptrdecl);
      f_rkttypes->addDep(tdtr->ptrdecl);
    }
    tdtr->declared = fully_declared;
  }
  return Language::typedefHandler(n);
}

int RACKET::classforwardDeclaration(Node *n) {
  String *name = Getattr(n, "sym:name");
  String *kind = Getattr(n, "kind");
  // Printf(stderr, "## fwd class %s\n", name);
  if (0) {
    Printf(stderr, "forward declaration of %s :: %s\n", name, kind);
  }
  declareForwardType(getTypeRecord(name, kind));
  return SWIG_OK;
}

int RACKET::enumforwardDeclaration(Node *n) {
  String *name = Getattr(n, "sym:name");
  // Printf(stderr, "## fwd enum %s\n", name);
  if (0) {
    Printf(stderr, "forward declaration of %s :: enum\n", name);
  }
  declareForwardType(getTypeRecord(name, "enum"));
  return SWIG_OK;
}

void RACKET::declareForwardType(TypeRecord *tr) {
  // Printf(stderr, "## FWD %s, %d\n", tr->ffitype, tr->declared);
  f_rkttypes->addDep(tr->decl);
  f_rkttypes->addDep(tr->ptrdecl);
}

int RACKET::enumDeclaration(Node *n) {
  if (getCurrentClass() && (cplus_mode != PUBLIC))
    return SWIG_NOWRAP;

  String *name = Getattr(n, "sym:name");
  // Printf(stderr, "## enum %s\n", name);
  TypeRecord *tr = getTypeRecord(name, "enum");
  String *tdname = Getattr(n, "tdname");
  if (tdname) { registerTypeRecord(tdname, tr); }
  int first = 1;

  tr->decl->reset();
  String *out = tr->decl->str;
  Printf(out, "(define %s\n", tr->ffitype);
  Printf(out, "  (_enum '(");
  for (Node *c = firstChild(n); c; c = nextSibling(c)) {
    String *slot_name = Getattr(c, "name");
    String *value = Getattr(c, "enumvalue");
    if (!first) { Printf(out, " "); } else { first = 0; }
    if (value && Strcmp(value, "")) {
      Printf(out, "%s = %s", slot_name, value);
    } else {
      Printf(out, "%s", slot_name);
    }
    Delete(value);
  }
  Printf(out, ")))\n");

  tr->declared = fully_declared;
  return SWIG_OK;
}


int RACKET::classDeclaration(Node *n) {
  String *name = Getattr(n, "sym:name");
  // Printf(stderr, "## class %s\n", name);
  String *kind = Getattr(n, "kind");
  TypeRecord *tr = getTypeRecord(name, kind);
  int result;

  String *tdname = Getattr(n, "tdname");
  if (tdname) { registerTypeRecord(tdname, tr); }

  scanForNested(n);

  struct member_ctx my_mctx = { NewList(), mctx };
  mctx = &my_mctx;
  result = Language::classDeclaration(n);
  mctx = my_mctx.prev;

  if (result != SWIG_OK) return result;

  if (!Strcmp(kind, "struct")) {
    tr->decl->reset();
    String *out = tr->decl->str;
    tr->declared = fully_declared;
    tr->setStructDefined();

    int first = 1;
    Printf(out, "(define-cstruct %s\n", tr->ffitype);
    Printf(out, "  (");
    for (Iterator iter = First(my_mctx.members); iter.item; iter = Next(iter)) {
      Node *c = iter.item;
      if (!Strcmp(nodeType(c), "cdecl")) {
        String *temp = Copy(Getattr(c, "decl"));
        if (temp) {
          Append(temp, Getattr(c, "type"));	//appending type to the end, otherwise wrong type
          String *ffitype = getRacketType(n, temp, tr->decl);
          Delete(temp);
          String *slotname = Getattr(c, "sym:name");
          if (!first) { writeIndent(out, 3, 0); } else { first = 0; }
          Printf(out, "[%s %s]", slotname, ffitype);
          Delete(ffitype);
        }
      } else {
        Printf(stderr, "Structure %s has a slot that we can't deal with.\n", name);
        Printf(stderr, "nodeType: %s, name: %s, type: %s\n",
               nodeType(c), Getattr(c, "name"), Getattr(c, "type"));
        SWIG_exit(EXIT_FAILURE);
      }
    }
    if (first) {
      // No fields. Add a fake zero-sized field to make Racket happy.
      Printf(out, "[__fake__ (_array _byte 0)]");
    }
    Printf(out, ")");
    write_wrapper_options(out, Getattr(n, "feature:struct-options"), 2,
                          tr->ffitype, Getattr(n, "name"));
    Printf(out, ")\n");

    f_rkttypes->addDep(tr->decl);
    return SWIG_OK;
  }
  else if (!Strcmp(kind, "union")) {
    tr->decl->reset();
    String *out = tr->decl->str;

    int first = 1;
    Printf(out, "(define %s\n", tr->ffitype);
    Printf(out, "  (_union ");
    for (Iterator iter = First(my_mctx.members); iter.item; iter = Next(iter)) {
      Node *c = iter.item;
      int indent = strlen("  (_union ");
      if (!Strcmp(nodeType(c), "cdecl")) {
        String *temp = Copy(Getattr(c, "decl"));
        if (temp) {
          Append(temp, Getattr(c, "type"));	//appending type to the end, otherwise wrong type
          String *ffitype = getRacketType(n, temp, tr->decl);
          Delete(temp);

          if (!first) { writeIndent(out, indent, 0); } else { first = 0; }
          Printf(out, "%s", ffitype);
          Delete(ffitype);
        }
      } else {
        Printf(stderr, "Union %s has a slot that we can't deal with.\n", Getattr(n, "name"));
        Printf(stderr, "nodeType: %s, name: %s, type: %s\n",
               nodeType(c), Getattr(c, "name"), Getattr(c, "type"));
        SWIG_exit(EXIT_FAILURE);
      }
    }
    Printf(out, "))\n");

    tr->declared = fully_declared;
    f_rkttypes->addDep(tr->decl);
    return SWIG_OK;
  }
  else {
    Printf(stderr, "Don't know how to deal with %s kind of class yet.\n", kind);
    Printf(stderr, " (name: %s)\n", name);
    SWIG_exit(EXIT_FAILURE);
    return 0; // unreachable (FIXME?)
  }
}

void RACKET::scanForNested(Node *n) {
  // Swig lifts nested struct decls *after* the outer struct decl, and fields in
  // the outer struct refer to the lifted struct via a typedef-like name (ie, no
  // "struct " prefix), which further confuses getTypeRecord. So when we hit a
  // class defn, scan ahead for lifted struct decls, marked with "nested" attr,
  // to register the type synonyms.
  n = nextSibling(n);
  while (n) {
    if (Strcmp(nodeType(n), "class")) break;
    if (!Getattr(n, "nested")) break;
    if (Getattr(n, "ScannedForNested")) break;
    Setattr(n, "ScannedForNesting", "1");
    String *name = Getattr(n, "sym:name");
    String *tdname = Getattr(n, "tdname");
    String *kind = Getattr(n, "kind");
    if (tdname) {
      TypeRecord *tr = getTypeRecord(name, kind);
      registerTypeRecord(tdname, tr);
    }
    n = nextSibling(n);
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

void RACKET::writeFunParams(File *out, Node *n, ParmList *pl, int indent, List *argouts, DeclItem *client) {
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
      String *ffitype = getRacketType(n, Getattr(p, "type"), client);
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

String *RACKET::getMappedType(Node *n, SwigType *ty) {
  Node *node = NewHash();
  Setattr(node, "type", ty);
  Setfile(node, Getfile(n));
  Setline(node, Getline(n));
  const String *tm = Swig_typemap_lookup("rktffi", node, "", 0);
  Delete(node);
  return tm ? NewString(tm) : NULL;
}

String *RACKET::getRacketType(Node *n, SwigType *ty0, DeclItem *client) {
  String *result;
  SwigType *ty = Copy(ty0);     // private copy for mutation; delete at end

  if ((result = getMappedType(n, ty))) {
    ;
  }
  else if (SwigType_isqualifier(ty)) {
    SwigType_del_qualifier(ty);
    result = getRacketType(n, ty, client);
  }
  else if (SwigType_isarray(ty)) {
    String *array_dim = SwigType_array_getdim(ty, 0);
    if (!Strcmp(array_dim, "")) {	//dimension less array convert to pointer
      Delete(array_dim);
      SwigType_del_array(ty);
      SwigType_add_pointer(ty);
      result = getRacketType(n, ty, client);
    } else if (SwigType_array_ndim(ty) == 1) {
      SwigType_pop_arrays(ty);
      String *innertype = getRacketType(n, ty, client);
      result = NewStringf("(_array %s %s)", innertype, convert_numeric_expr(Char(array_dim)));
      Delete(array_dim);
      Delete(innertype);
    } else {
      String *innertype = getRacketType(n, ty, client);
      result = NewStringf("(_array %s (FIXME #| %s |#))", innertype, array_dim);
      Delete(innertype);
    }
  }
  else if (SwigType_isfunction(ty)) {
    // Unlike function decl, don't use "rktin", "rktargout" typemaps.
    SwigType *fn = SwigType_pop_function(ty);
    ParmList *pl = SwigType_function_parms(fn, n);
    String *restype = getRacketType(n, ty, client);
    String *out = NewString("");
    Printf(out, "(_fun ");
    writeFunParams(out, n, pl, -1, NULL, client);
    Printf(out, "-> %s)", restype);
    Delete(fn);
    Delete(restype);
    return out;
  }
  else if (SwigType_ispointer(ty)) {
    SwigType_del_pointer(ty);
    while (SwigType_isqualifier(ty)) {
      SwigType_del_qualifier(ty);
    }
    // Precedence: typemap, function, default
    String *inner = getMappedType(n, ty);
    if (!inner && SwigType_isfunction(ty)) {
      result = getRacketType(n, ty, client);
    } else {
      if (!inner) {
        inner = getRacketType(n, ty, SwigType_issimple(ty) ? NULL : client);
      }
      TypeRecord *innertr = getRacketTypeRecord(inner);
      if (innertr && innertr->ptrtype) {
        result = Copy(innertr->ptrtype);
        if (client) { client->addDep(innertr->ptrdecl); }
      } else {
        result = NewStringf("(_pointer-to %s)", inner);
      }
      Delete(inner);
    }
  }
  else if (SwigType_issimple(ty)) {
    String *str = SwigType_str(ty, 0);
    TypeRecord *tr = getTypeRecord(str);
    if (tr->declared == undeclared) {
      declareForwardType(tr);
    }
    result = Copy(tr->ffitype);
    if (client) { client->addDep(tr->decl); }
  }
  else {
    result = NewStringf("(_FIXME #| %s |#)", SwigType_str(ty, 0));
  }

  Delete(ty);
  return result;
}

// ============================================================

void writeFunPrefix(File *out, String *prefix, int indent) {
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
  int existing_indent = 0;
  while (isspace(s[0]) || (s[0] == '\n')) { // FIXME: is isspace('\n') true?
    if (s[0] == '\n') { existing_indent = 0; } else { existing_indent++; }
    s++; len--;
  }
  while (isspace(s[len - 1]) || (s[len - 1] == '\n')) { len--; }
  String *tm = NewString("");
  Write(tm, s, len);
  if (indent >= 0) {
    String *indentation = NewString("\n");
    indent = indent - existing_indent;
    while (indent > 0) { Printf(indentation, " "); indent--; }
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

enum define_types_t getDefineTypesMode(Node *n) {
  String *m = Getattr(n, "feature:DefineTypesMode");
  if (!m || !Strcmp(m, "all")) {
    return define_all;
  } else if (!Strcmp(m, "pointer")) {
    return define_ptr;
  } else if (!Strcmp(m, "none")) {
    return define_none;
  } else {
    return define_all;
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
  int read = 0, nlen = 0;
  char op = 0;

  intptr_t ivalue;
  double dvalue;
  char svalue[10];

 READ_NUM:
  sscanf(s, " %n", &read);
  s = s + read; read = 0;
  if (sscanf(s, "%ji%1[uU]%2[lL]%n", &ivalue, &svalue[0], &svalue[2], &read) == 3) {
    goto GOT_INTEGER;
  } else if (sscanf(s, "%ji%2[lL]%n", &ivalue, &svalue[0], &read) == 2) {
    goto GOT_INTEGER;
  } else if ((sscanf(s, "%ji%n", &ivalue, &read) == 1) && (s[read] !='.')) {
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
  nlen = read;
  while (1) {
    switch (s[nlen - 1]) {
    case 'u': case 'U': case 'l': case 'L':
      nlen--;
      continue;
    default:
      ;
    }
    break;
  }
  if (nlen == 1 || s[0] != '0') {
    // decimal
    Write(out, s, nlen);
  } else if (s[1] == 'x' || s[1] == 'X') {
    // hexadecimal
    Printf(out, "#x");
    Write(out, s + 2, nlen - 2);
  } else {
    // octal
    Printf(out, "#o");
    Write(out, s + 1, nlen - 1);
  }
  Printf(out, " ");
  s = s + read;
  goto READ_OP;

 READ_OP:
  sscanf(s, " %n", &read);
  s = s + read; read = 0;
  if (s == end) {
    goto SUCCESS;
  } else if (sscanf(s, "%1[+*/|&%^-]%n", &svalue[0], &read)) {
    if (!op || op == svalue[0]) {
      op = svalue[0];
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
  } else {
    const char *f;
    switch (op) {
    case '+': f = "+"; break;
    case '-': f = "-"; break;
    case '*': f = "*"; break;
    case '/': f = "/"; break; // FIXME: floating-point '/' vs integer 'quotient'
    case '%': f = "modulo"; break;
    case '|': f = "bitwise-ior"; break;
    case '&': f = "bitwise-and"; break;
    case '^': f = "bitwise-xor"; break;
    default: goto ERROR;
    }
    String *result = NewStringf("(%s %s)", f, out);
    Delete(out);
    return result;
  }
}

// ============================================================

extern "C" Language *swig_racket(void) {
  return new RACKET();
}
