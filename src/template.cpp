#include "template.h"

#include <stdio.h>
#include <stdarg.h>

#include <qlist.h>
#include <qarray.h>
#include <qdict.h>
#include <qstrlist.h>
#include <qvaluelist.h>
#include <qstack.h>
#include <qfile.h>
#include <qregexp.h>

#include "sortdict.h"
#include "ftextstream.h"
#include "message.h"
#include "util.h"

#define ENABLE_TRACING 0

#if ENABLE_TRACING
#define TRACE(x) printf x
#else
#define TRACE(x)
#endif

class TemplateToken;

//-------------------------------------------------------------------

static QValueList<QCString> split(const QCString &str,const QCString &sep,
                                  bool allowEmptyEntries=FALSE,bool cleanup=TRUE)
{
  QValueList<QCString> lst;

  int j = 0;
  int i = str.find( sep, j );

  while (i!=-1)
  {
    if ( str.mid(j,i-j).length() > 0 )
    {
      if (cleanup)
      {
        lst.append(str.mid(j,i-j).stripWhiteSpace());
      }
      else
      {
        lst.append(str.mid(j,i-j));
      }
    }
    else if (allowEmptyEntries)
    {
      lst.append("");
    }
    j = i + sep.length();
    i = str.find(sep,j);
  }

  int l = str.length() - 1;
  if (str.mid(j,l-j+1).length()>0)
  {
    if (cleanup)
    {
      lst.append(str.mid(j,l-j+1).stripWhiteSpace());
    }
    else
    {
      lst.append(str.mid(j,l-j+1));
    }
  }
  else if (allowEmptyEntries)
  {
    lst.append("");
  }

  return lst;
}

//----------------------------------------------------------------------------

#if ENABLE_TRACING
static QCString replace(const char *s,char csrc,char cdst)
{
  QCString result = s;
  for (char *p=result.data();*p;p++)
  {
    if (*p==csrc) *p=cdst;
  }
  return result;
}
#endif

//- TemplateVariant implementation -------------------------------------------

/** @brief Private data of a template variant object */
class TemplateVariant::Private
{
  public:
    Private() : raw(FALSE) {}
    Type                type;
    int                 intVal;
    QCString            strVal;
    bool                boolVal;
    const TemplateStructIntf *strukt;
    const TemplateListIntf   *list;
    Delegate            delegate;
    bool                raw;
};

TemplateVariant::TemplateVariant()
{
  p = new Private;
  p->type=None;
}

TemplateVariant::TemplateVariant(bool b)
{
  p = new Private;
  p->type = Bool;
  p->boolVal = b;
}

TemplateVariant::TemplateVariant(int v)
{
  p = new Private;
  p->type = Integer;
  p->intVal = v;
}

TemplateVariant::TemplateVariant(const char *s,bool raw)
{
  p = new Private;
  p->type = String;
  p->strVal = s;
  p->raw = raw;
}

TemplateVariant::TemplateVariant(const QCString &s,bool raw)
{
  p = new Private;
  p->type = String;
  p->strVal = s;
  p->raw = raw;
}

TemplateVariant::TemplateVariant(const TemplateStructIntf *s)
{
  p = new Private;
  p->type = Struct;
  p->strukt = s; }

TemplateVariant::TemplateVariant(const TemplateListIntf *l)
{
  p = new Private;
  p->type = List;
  p->list = l;
}

TemplateVariant::TemplateVariant(const TemplateVariant::Delegate &delegate)
{
  p = new Private;
  p->type = Function;
  p->delegate = delegate;
}

TemplateVariant::~TemplateVariant()
{
  delete p;
}

TemplateVariant::TemplateVariant(const TemplateVariant &v)
{
  p = new Private;
  p->type    = v.p->type;
  p->raw     = v.p->raw;
  switch (p->type)
  {
    case None: break;
    case Bool:     p->boolVal = v.p->boolVal; break;
    case Integer:  p->intVal  = v.p->intVal;  break;
    case String:   p->strVal  = v.p->strVal;  break;
    case Struct:   p->strukt  = v.p->strukt;  break;
    case List:     p->list    = v.p->list;    break;
    case Function: p->delegate= v.p->delegate;break;
  }
}

TemplateVariant &TemplateVariant::operator=(const TemplateVariant &v)
{
  p->type    = v.p->type;
  p->raw     = v.p->raw;
  switch (p->type)
  {
    case None: break;
    case Bool:     p->boolVal = v.p->boolVal; break;
    case Integer:  p->intVal  = v.p->intVal;  break;
    case String:   p->strVal  = v.p->strVal;  break;
    case Struct:   p->strukt  = v.p->strukt;  break;
    case List:     p->list    = v.p->list;    break;
    case Function: p->delegate= v.p->delegate;break;
  }
  return *this;
}

QCString TemplateVariant::toString() const
{
  QCString result;
  switch (p->type)
  {
    case None:
      break;
    case Bool:
      result=p->boolVal ? "true" : "false";
      break;
    case Integer:
      result=QCString().setNum(p->intVal);
      break;
    case String:
      result=p->strVal;
      break;
    case Struct:
      result="[struct]";
      break;
    case List:
      result="[list]";
      break;
    case Function:
      result="[function]";
      break;
  }
  return result;
}

bool TemplateVariant::toBool() const
{
  bool result=FALSE;
  switch (p->type)
  {
    case None:
      break;
    case Bool:
      result = p->boolVal;
      break;
    case Integer:
      result = p->intVal!=0;
      break;
    case String:
      result = !p->strVal.isEmpty(); // && p->strVal!="false" && p->strVal!="0";
      break;
    case Struct:
      result = TRUE;
      break;
    case List:
      result = p->list->count()!=0;
      break;
    case Function:
      result = FALSE;
      break;
  }
  return result;
}

int TemplateVariant::toInt() const
{
  int result=0;
  switch (p->type)
  {
    case None:
      break;
    case Bool:
      result = p->boolVal ? 1 : 0;
      break;
    case Integer:
      result = p->intVal;
      break;
    case String:
      result = p->strVal.toInt();
      break;
    case Struct:
      break;
    case List:
      result = p->list->count();
      break;
    case Function:
      result = 0;
      break;
  }
  return result;
}

const TemplateStructIntf *TemplateVariant::toStruct() const
{
  return p->type==Struct ? p->strukt : 0;
}

const TemplateListIntf *TemplateVariant::toList() const
{
  return p->type==List ? p->list : 0;
}

TemplateVariant TemplateVariant::call(const QValueList<TemplateVariant> &args)
{
  if (p->type==Function) return p->delegate(args);
  return TemplateVariant();
}

bool TemplateVariant::operator==(TemplateVariant &other)
{
  if (p->type==None)
  {
    return FALSE;
  }
  if (p->type==TemplateVariant::List && other.p->type==TemplateVariant::List)
  {
    return p->list==other.p->list; // TODO: improve me
  }
  else if (p->type==TemplateVariant::Struct && other.p->type==TemplateVariant::Struct)
  {
    return p->strukt==other.p->strukt; // TODO: improve me
  }
  else
  {
    return toString()==other.toString();
  }
}

TemplateVariant::Type TemplateVariant::type() const
{
  return p->type;
}

bool TemplateVariant::isValid() const
{
  return p->type!=None;
}

void TemplateVariant::setRaw(bool b)
{
  p->raw = b;
}

bool TemplateVariant::raw() const
{
  return p->raw;
}

//- Template struct implementation --------------------------------------------


/** @brief Private data of a template struct object */
class TemplateStruct::Private
{
  public:
    Private() : fields(17)
    { fields.setAutoDelete(TRUE); }
    QDict<TemplateVariant> fields;
};

TemplateStruct::TemplateStruct()
{
  p = new Private;
}

TemplateStruct::~TemplateStruct()
{
  delete p;
}

void TemplateStruct::set(const char *name,const TemplateVariant &v)
{
  TemplateVariant *pv = p->fields.find(name);
  if (pv) // change existing field
  {
    *pv = v;
  }
  else // insert new field
  {
    p->fields.insert(name,new TemplateVariant(v));
  }
}

TemplateVariant TemplateStruct::get(const char *name) const
{
  TemplateVariant *v = p->fields.find(name);
  return v ? *v : TemplateVariant();
}

//- Template list implementation ----------------------------------------------


/** @brief Private data of a template list object */
class TemplateList::Private
{
  public:
    Private() : index(-1) {}
    QValueList<TemplateVariant> elems;
    int index;
};


TemplateList::TemplateList()
{
  p = new Private;
}

TemplateList::~TemplateList()
{
  delete p;
}

int TemplateList::count() const
{
  return p->elems.count();
}

void TemplateList::append(const TemplateVariant &v)
{
  p->elems.append(v);
}

// iterator support
class TemplateListConstIterator : public TemplateListIntf::ConstIterator
{
  public:
    TemplateListConstIterator(const TemplateList &l) : m_list(l) { m_index=-1; }
    virtual ~TemplateListConstIterator() {}
    virtual void toFirst()
    {
      m_it = m_list.p->elems.begin();
      m_index=0;
    }
    virtual void toLast()
    {
      m_it = m_list.p->elems.fromLast();
      m_index=m_list.count()-1;
    }
    virtual void toNext()
    {
      if (m_it!=m_list.p->elems.end())
      {
        ++m_it;
        ++m_index;
      }
    }
    virtual void toPrev()
    {
      if (m_index>0)
      {
        --m_it;
        --m_index;
      }
      else
      {
        m_index=-1;
      }
    }
    virtual bool current(TemplateVariant &v) const
    {
      if (m_index<0 || m_it==m_list.p->elems.end())
      {
        v = TemplateVariant();
        return FALSE;
      }
      else
      {
        v = *m_it;
        return TRUE;
      }
    }
  private:
    const TemplateList &m_list;
    QValueList<TemplateVariant>::ConstIterator m_it;
    int m_index;
};

TemplateListIntf::ConstIterator *TemplateList::createIterator() const
{
  return new TemplateListConstIterator(*this);
}

TemplateVariant TemplateList::at(int index) const
{
  if (index>=0 && index<(int)p->elems.count())
  {
    return p->elems[index];
  }
  else
  {
    return TemplateVariant();
  }
}

//- Operator types ------------------------------------------------------------

/** @brief Class representing operators that can appear in template expressions */
class Operator
{
  public:
      /* Operator precedence (low to high)
         or
         and
         not
         in
         ==, !=, <, >, <=, >=
         |
         :
         ,
       */
    enum Type
    {
      Or, And, Not, In, Equal, NotEqual, Less, Greater, LessEqual,
      GreaterEqual, Filter, Colon, Comma, Last
    };

    static const char *toString(Type op)
    {
      switch(op)
      {
        case Or:           return "or";
        case And:          return "and";
        case Not:          return "not";
        case In:           return "in";
        case Equal:        return "==";
        case NotEqual:     return "!=";
        case Less:         return "<";
        case Greater:      return ">";
        case LessEqual:    return "<=";
        case GreaterEqual: return ">=";
        case Filter:       return "|";
        case Colon:        return ":";
        case Comma:        return ",";
        case Last:         return "?";
      }
      return "?";
    }
};

//-----------------------------------------------------------------------------

class TemplateNodeBlock;

/** @brief Class holding stacks of blocks available in the context */
class TemplateBlockContext
{
  public:
    TemplateBlockContext();
    TemplateNodeBlock *get(const QCString &name) const;
    TemplateNodeBlock *pop(const QCString &name) const;
    void add(TemplateNodeBlock *block);
    void add(TemplateBlockContext *ctx);
    void push(TemplateNodeBlock *block);
    void clear();
  private:
    QDict< QList<TemplateNodeBlock> > m_blocks;
};


/** @brief Internal class representing the implementation of a template
 *  context */
class TemplateContextImpl : public TemplateContext
{
  public:
    TemplateContextImpl(const TemplateEngine *e);
    virtual ~TemplateContextImpl();

    // TemplateContext methods
    void push();
    void pop();
    void set(const char *name,const TemplateVariant &v);
    TemplateVariant get(const QCString &name) const;
    const TemplateVariant *getRef(const QCString &name) const;
    void setOutputDirectory(const QCString &dir)
    { m_outputDir = dir; }
    void setEscapeIntf(TemplateEscapeIntf *intf)
    { m_escapeIntf = intf; }
    void setSpacelessIntf(TemplateSpacelessIntf *intf)
    { m_spacelessIntf = intf; }

    // internal methods
    TemplateBlockContext *blockContext();
    TemplateVariant getPrimary(const QCString &name) const;
    void setLocation(const QCString &templateName,int line)
    { m_templateName=templateName; m_line=line; }
    QCString templateName() const { return m_templateName; }
    int line() const { return m_line; }
    QCString outputDirectory() const { return m_outputDir; }
    TemplateEscapeIntf *escapeIntf() const { return m_escapeIntf; }
    TemplateSpacelessIntf *spacelessIntf() const { return m_spacelessIntf; }
    void enableSpaceless(bool b) { m_spacelessEnabled=b; }
    bool spacelessEnabled() const { return m_spacelessEnabled && m_spacelessIntf; }
    void warn(const char *fileName,int line,const char *fmt,...) const;

  private:
    const TemplateEngine *m_engine;
    QCString m_templateName;
    int m_line;
    QCString m_outputDir;
    QList< QDict<TemplateVariant> > m_contextStack;
    TemplateBlockContext m_blockContext;
    TemplateEscapeIntf *m_escapeIntf;
    TemplateSpacelessIntf *m_spacelessIntf;
    bool m_spacelessEnabled;
};

//-----------------------------------------------------------------------------

/** @brief The implementation of the "add" filter */
class FilterAdd
{
  public:
    static int variantIntValue(const TemplateVariant &v,bool &isInt)
    {
      isInt = v.type()==TemplateVariant::Integer;
      if (!isInt && v.type()==TemplateVariant::String)
      {
        return v.toString().toInt(&isInt);
      }
      return isInt ? v.toInt() : 0;
    }
    static TemplateVariant apply(const TemplateVariant &v,const TemplateVariant &arg)
    {
      if (!v.isValid())
      {
        return arg;
      }
      bool lhsIsInt;
      int  lhsValue = variantIntValue(v,lhsIsInt);
      bool rhsIsInt;
      int  rhsValue = variantIntValue(arg,rhsIsInt);
      if (lhsIsInt && rhsIsInt)
      {
        return lhsValue+rhsValue;
      }
      else if (v.type()==TemplateVariant::String && arg.type()==TemplateVariant::String)
      {
        return TemplateVariant(v.toString() + arg.toString());
      }
      else
      {
        return v;
      }
    }
};

//-----------------------------------------------------------------------------

/** @brief The implementation of the "prepend" filter */
class FilterPrepend
{
  public:
    static TemplateVariant apply(const TemplateVariant &v,const TemplateVariant &arg)
    {
      if (v.type()==TemplateVariant::String && arg.type()==TemplateVariant::String)
      {
        return TemplateVariant(arg.toString() + v.toString());
      }
      else
      {
        return v;
      }
    }
};

//--------------------------------------------------------------------

/** @brief The implementation of the "length" filter */
class FilterLength
{
  public:
    static TemplateVariant apply(const TemplateVariant &v,const TemplateVariant &)
    {
      if (!v.isValid())
      {
        return TemplateVariant();
      }
      if (v.type()==TemplateVariant::List)
      {
        return TemplateVariant(v.toList()->count());
      }
      else if (v.type()==TemplateVariant::String)
      {
        return TemplateVariant((int)v.toString().length());
      }
      else
      {
        return TemplateVariant();
      }
    }
};

//--------------------------------------------------------------------

/** @brief The implementation of the "default" filter */
class FilterDefault
{
  public:
    static TemplateVariant apply(const TemplateVariant &v,const TemplateVariant &arg)
    {
      if (!v.isValid())
      {
        return arg;
      }
      else if (v.type()==TemplateVariant::String && v.toString().isEmpty())
      {
        return arg;
      }
      else
      {
        return v;
      }
    }
};

//--------------------------------------------------------------------

/** @brief The implementation of the "default" filter */
class FilterStripPath
{
  public:
    static TemplateVariant apply(const TemplateVariant &v,const TemplateVariant &)
    {
      if (!v.isValid() || v.type()!=TemplateVariant::String)
      {
        return v;
      }
      QCString result = v.toString();
      int i=result.findRev('/');
      if (i!=-1)
      {
        result=result.mid(i+1);
      }
      i=result.findRev('\\');
      if (i!=-1)
      {
        result=result.mid(i+1);
      }
      return result;
    }
};

//--------------------------------------------------------------------

/** @brief The implementation of the "default" filter */
class FilterNoWrap
{
  public:
    static TemplateVariant apply(const TemplateVariant &v,const TemplateVariant &)
    {
      if (!v.isValid() || v.type()!=TemplateVariant::String)
      {
        return v;
      }
      QCString s = v.toString();
      return substitute(s," ","&#160;");
    }
};

//--------------------------------------------------------------------

/** @brief The implementation of the "divisibleby" filter */
class FilterDivisibleBy
{
  public:
    static TemplateVariant apply(const TemplateVariant &v,const TemplateVariant &n)
    {
      if (!v.isValid() || !n.isValid())
      {
        return TemplateVariant();
      }
      if (v.type()==TemplateVariant::Integer && n.type()==TemplateVariant::Integer)
      {
        return TemplateVariant((v.toInt()%n.toInt())==0);
      }
      else
      {
        return TemplateVariant();
      }
    }
};


//--------------------------------------------------------------------

/** @brief Factory singleton for registering and creating filters */
class TemplateFilterFactory
{
  public:
    typedef TemplateVariant (FilterFunction)(const TemplateVariant &v,const TemplateVariant &arg);

    static TemplateFilterFactory *instance()
    {
      static TemplateFilterFactory *instance = 0;
      if (instance==0) instance = new TemplateFilterFactory;
      return instance;
    }

    TemplateVariant apply(const QCString &name,const TemplateVariant &v,const TemplateVariant &arg, bool &ok)
    {
      FilterFunction *func = (FilterFunction*)m_registry.find(name);
      if (func)
      {
        ok=TRUE;
        return (*func)(v,arg);
      }
      else
      {
        ok=FALSE;
        return v;
      }
    }

    void registerFilter(const QCString &name,FilterFunction *func)
    {
      m_registry.insert(name,(void*)func);
    }

    /** @brief Helper class for registering a filter function */
    template<class T> class AutoRegister
    {
      public:
        AutoRegister<T>(const QCString &key)
        {
          TemplateFilterFactory::instance()->registerFilter(key,&T::apply);
        }
    };

  private:
    QDict<void> m_registry;
};

// register a handlers for each filter we support
static TemplateFilterFactory::AutoRegister<FilterAdd>         fAdd("add");
static TemplateFilterFactory::AutoRegister<FilterAdd>         fAppend("append");
static TemplateFilterFactory::AutoRegister<FilterLength>      fLength("length");
static TemplateFilterFactory::AutoRegister<FilterNoWrap>      fNoWrap("nowrap");
static TemplateFilterFactory::AutoRegister<FilterDefault>     fDefault("default");
static TemplateFilterFactory::AutoRegister<FilterPrepend>     fPrepend("prepend");
static TemplateFilterFactory::AutoRegister<FilterStripPath>   fStripPath("stripPath");
static TemplateFilterFactory::AutoRegister<FilterDivisibleBy> fDivisibleBy("divisibleby");

//--------------------------------------------------------------------

/** @brief Base class for all nodes in the abstract syntax tree of an
 *  expression.
 */
class ExprAst
{
  public:
    virtual ~ExprAst() {}
    virtual TemplateVariant resolve(TemplateContext *) { return TemplateVariant(); }
};

/** @brief Class representing a number in the AST */
class ExprAstNumber : public ExprAst
{
  public:
    ExprAstNumber(int num) : m_number(num)
    { TRACE(("ExprAstNumber(%d)\n",num)); }
    int number() const { return m_number; }
    virtual TemplateVariant resolve(TemplateContext *) { return TemplateVariant(m_number); }
  private:
    int m_number;
};

/** @brief Class representing a variable in the AST */
class ExprAstVariable : public ExprAst
{
  public:
    ExprAstVariable(const char *name) : m_name(name)
    { TRACE(("ExprAstVariable(%s)\n",name)); }
    const QCString &name() const { return m_name; }
    virtual TemplateVariant resolve(TemplateContext *c)
    {
      TemplateContextImpl *ci = dynamic_cast<TemplateContextImpl*>(c);
      TemplateVariant v = c->get(m_name);
      if (!v.isValid())
      {
        ci->warn(ci->templateName(),ci->line(),"undefined variable '%s' in expression",m_name.data());
      }
      return v;
    }
  private:
    QCString m_name;
};

class ExprAstFunctionVariable : public ExprAst
{
  public:
    ExprAstFunctionVariable(ExprAst *var,const QList<ExprAst> &args)
      : m_var(var), m_args(args)
    { TRACE(("ExprAstFunctionVariable(%s)\n",var->name().data()));
      m_args.setAutoDelete(TRUE);
    }
    virtual TemplateVariant resolve(TemplateContext *c)
    {
      QValueList<TemplateVariant> args;
      for (uint i=0;i<m_args.count();i++)
      {
        TemplateVariant v = m_args.at(i)->resolve(c);
        args.append(v);
      }
      TemplateVariant v = m_var->resolve(c);
      if (v.type()==TemplateVariant::Function)
      {
        v = v.call(args);
      }
      return v;
    }
  private:
    ExprAst *m_var;
    QList<ExprAst> m_args;
};

/** @brief Class representing a filter in the AST */
class ExprAstFilter : public ExprAst
{
  public:
    ExprAstFilter(const char *name,ExprAst *arg) : m_name(name), m_arg(arg)
    { TRACE(("ExprAstFilter(%s)\n",name)); }
   ~ExprAstFilter() { delete m_arg; }
    const QCString &name() const { return m_name; }
    TemplateVariant apply(const TemplateVariant &v,TemplateContext *c)
    {
      TemplateContextImpl *ci = dynamic_cast<TemplateContextImpl*>(c);
      TRACE(("Applying filter '%s' to '%s' (type=%d)\n",m_name.data(),v.toString().data(),v.type()));
      TemplateVariant arg;
      if (m_arg) arg = m_arg->resolve(c);
      bool ok;
      TemplateVariant result = TemplateFilterFactory::instance()->apply(m_name,v,arg,ok);
      if (!ok)
      {
        ci->warn(ci->templateName(),ci->line(),"unknown filter '%s'",m_name.data());
      }
      return result;
    }
  private:
    QCString m_name;
    ExprAst *m_arg;
};

/** @brief Class representing a filter applied to an expression in the AST */
class ExprAstFilterAppl : public ExprAst
{
  public:
    ExprAstFilterAppl(ExprAst *expr,ExprAstFilter *filter)
      : m_expr(expr), m_filter(filter)
    { TRACE(("ExprAstFilterAppl\n")); }
   ~ExprAstFilterAppl() { delete m_expr; delete m_filter; }
    virtual TemplateVariant resolve(TemplateContext *c)
    {
      return m_filter->apply(m_expr->resolve(c),c);
    }
  private:
    ExprAst *m_expr;
    ExprAstFilter *m_filter;
};

/** @brief Class representing a string literal in the AST */
class ExprAstLiteral : public ExprAst
{
  public:
    ExprAstLiteral(const char *lit) : m_literal(lit)
    { TRACE(("ExprAstLiteral(%s)\n",lit)); }
    const QCString &literal() const { return m_literal; }
    virtual TemplateVariant resolve(TemplateContext *) { return TemplateVariant(m_literal); }
  private:
    QCString m_literal;
};

/** @brief Class representing a negation (not) operator in the AST */
class ExprAstNegate : public ExprAst
{
  public:
    ExprAstNegate(ExprAst *expr) : m_expr(expr)
    { TRACE(("ExprAstNegate\n")); }
   ~ExprAstNegate() { delete m_expr; }
    virtual TemplateVariant resolve(TemplateContext *c)
    { return TemplateVariant(!m_expr->resolve(c).toBool()); }
  private:
    ExprAst *m_expr;
};

/** @brief Class representing a binary operator in the AST */
class ExprAstBinary : public ExprAst
{
  public:
    ExprAstBinary(Operator::Type op,ExprAst *lhs,ExprAst *rhs)
      : m_operator(op), m_lhs(lhs), m_rhs(rhs)
    { TRACE(("ExprAstBinary %s\n",Operator::toString(op))); }
   ~ExprAstBinary() { delete m_lhs; delete m_rhs; }
    virtual TemplateVariant resolve(TemplateContext *c)
    {
      TemplateVariant lhs = m_lhs->resolve(c);
      TemplateVariant rhs = m_rhs ? m_rhs->resolve(c) : TemplateVariant();
      switch(m_operator)
      {
        case Operator::Or:
          return TemplateVariant(lhs.toBool() || rhs.toBool());
        case Operator::And:
          return TemplateVariant(lhs.toBool() && rhs.toBool());
        case Operator::Equal:
          return TemplateVariant(lhs == rhs);
        case Operator::NotEqual:
          return TemplateVariant(!(lhs == rhs));
        case Operator::Less:
          if (lhs.type()==TemplateVariant::String && rhs.type()==TemplateVariant::String)
          {
            return lhs.toString()<rhs.toString();
          }
          else
          {
            return lhs.toInt()<rhs.toInt();
          }
        case Operator::Greater:
          if (lhs.type()==TemplateVariant::String && rhs.type()==TemplateVariant::String)
          {
            return !(lhs.toString()<rhs.toString());
          }
          else
          {
            return lhs.toInt()>rhs.toInt();
          }
        case Operator::LessEqual:
          if (lhs.type()==TemplateVariant::String && rhs.type()==TemplateVariant::String)
          {
            return lhs.toString()==rhs.toString() || lhs.toString()<rhs.toString();
          }
          else
          {
            return lhs.toInt()<=rhs.toInt();
          }
        case Operator::GreaterEqual:
          if (lhs.type()==TemplateVariant::String && rhs.type()==TemplateVariant::String)
          {
            return lhs.toString()==rhs.toString() || !(lhs.toString()<rhs.toString());
          }
          else
          {
            return lhs.toInt()>=rhs.toInt();
          }
        default:
          return TemplateVariant();
      }
    }
  private:
    Operator::Type m_operator;
    ExprAst *m_lhs;
    ExprAst *m_rhs;
};

//----------------------------------------------------------

/** @brief Base class of all nodes in a template's AST */
class TemplateNode
{
  public:
    TemplateNode(TemplateNode *parent) : m_parent(parent) {}
    virtual ~TemplateNode() {}

    virtual void render(FTextStream &ts, TemplateContext *c) = 0;

    TemplateNode *parent() { return m_parent; }

  private:
    TemplateNode *m_parent;
};

//----------------------------------------------------------

/** @brief Parser for templates */
class TemplateParser
{
  public:
    TemplateParser(const TemplateEngine *engine,
                   const QCString &templateName,QList<TemplateToken> &tokens);
    void parse(TemplateNode *parent,int line,const QStrList &stopAt,
               QList<TemplateNode> &nodes);
    bool hasNextToken() const;
    TemplateToken *takeNextToken();
    void removeNextToken();
    void prependToken(const TemplateToken *token);
    const TemplateToken *currentToken() const;
    QCString templateName() const { return m_templateName; }
    void warn(const char *fileName,int line,const char *fmt,...) const;
  private:
    const TemplateEngine *m_engine;
    QCString m_templateName;
    QList<TemplateToken> &m_tokens;
};

//--------------------------------------------------------------------

/** @brief Recursive decent parser for Django style template expressions.
 */
class ExpressionParser
{
  public:
    ExpressionParser(const TemplateParser *parser,int line)
      : m_parser(parser), m_line(line), m_tokenStream(0)
    {
    }
    virtual ~ExpressionParser()
    {
    }

    ExprAst *parse(const char *expr)
    {
      if (expr==0) return 0;
      m_tokenStream = expr;
      getNextToken();
      return parseOrExpression();
    }

    ExprAst *parsePrimary(const char *expr)
    {
      if (expr==0) return 0;
      m_tokenStream = expr;
      getNextToken();
      return parsePrimaryExpression();
    }

    ExprAst *parseVariable(const char *varExpr)
    {
      if (varExpr==0) return 0;
      m_tokenStream = varExpr;
      getNextToken();
      return parseFilteredVariable();
    }

  private:

    /** @brief Class representing a token within an expression. */
    class ExprToken
    {
      public:
        ExprToken() : type(Unknown), num(-1), op(Operator::Or)
        {
        }
        enum Type
        {
          Unknown, Operator, Number, Identifier, Literal
        };

        Type type;
        int num;
        QCString id;
        Operator::Type op;
    };

    ExprAst *parseOrExpression()
    {
      TRACE(("{parseOrExpression(%s)\n",m_tokenStream));
      ExprAst *lhs = parseAndExpression();
      if (lhs)
      {
        while (m_curToken.type==ExprToken::Operator &&
            m_curToken.op==Operator::Or)
        {
          getNextToken();
          ExprAst *rhs = parseAndExpression();
          lhs = new ExprAstBinary(Operator::Or,lhs,rhs);
        }
      }
      TRACE(("}parseOrExpression(%s)\n",m_tokenStream));
      return lhs;
    }

    ExprAst *parseAndExpression()
    {
      TRACE(("{parseAndExpression(%s)\n",m_tokenStream));
      ExprAst *lhs = parseNotExpression();
      if (lhs)
      {
        while (m_curToken.type==ExprToken::Operator &&
               m_curToken.op==Operator::And)
        {
          getNextToken();
          ExprAst *rhs = parseNotExpression();
          lhs = new ExprAstBinary(Operator::And,lhs,rhs);
        }
      }
      TRACE(("}parseAndExpression(%s)\n",m_tokenStream));
      return lhs;
    }

    ExprAst *parseNotExpression()
    {
      TRACE(("{parseNotExpression(%s)\n",m_tokenStream));
      ExprAst *result=0;
      if (m_curToken.type==ExprToken::Operator &&
          m_curToken.op==Operator::Not)
      {
        getNextToken();
        ExprAst *expr = parseCompareExpression();
        if (expr==0)
        {
          warn(m_parser->templateName(),m_line,"argument missing for not operator");
          return 0;
        }
        result = new ExprAstNegate(expr);
      }
      else
      {
        result = parseCompareExpression();
      }
      TRACE(("}parseNotExpression(%s)\n",m_tokenStream));
      return result;
    }

    ExprAst *parseCompareExpression()
    {
      TRACE(("{parseCompareExpression(%s)\n",m_tokenStream));
      ExprAst *lhs = parsePrimaryExpression();
      if (lhs)
      {
        Operator::Type op = m_curToken.op;
        if (m_curToken.type==ExprToken::Operator &&
            (op==Operator::Less      ||
             op==Operator::Greater   ||
             op==Operator::Equal     ||
             op==Operator::NotEqual  ||
             op==Operator::LessEqual ||
             op==Operator::GreaterEqual
            )
           )
        {
          getNextToken();
          ExprAst *rhs = parseNotExpression();
          lhs = new ExprAstBinary(op,lhs,rhs);
        }
      }
      TRACE(("}parseCompareExpression(%s)\n",m_tokenStream));
      return lhs;
    }

    ExprAst *parsePrimaryExpression()
    {
      TRACE(("{parsePrimary(%s)\n",m_tokenStream));
      ExprAst *result=0;
      switch (m_curToken.type)
      {
        case ExprToken::Number:
          result = parseNumber();
          break;
        case ExprToken::Identifier:
          result = parseFilteredVariable();
          break;
        case ExprToken::Literal:
          result = parseLiteral();
          break;
        default:
          if (m_curToken.type==ExprToken::Operator)
          {
            warn(m_parser->templateName(),m_line,"unexpected operator '%s' in expression",
                Operator::toString(m_curToken.op));
          }
          else
          {
            warn(m_parser->templateName(),m_line,"unexpected token in expression");
          }
      }
      TRACE(("}parsePrimary(%s)\n",m_tokenStream));
      return result;
    }

    ExprAst *parseNumber()
    {
      TRACE(("{parseNumber(%d)\n",m_curToken.num));
      ExprAst *num = new ExprAstNumber(m_curToken.num);
      getNextToken();
      TRACE(("}parseNumber()\n"));
      return num;
    }

    ExprAst *parseIdentifier()
    {
      TRACE(("{parseIdentifier(%s)\n",m_curToken.id.data()));
      ExprAst *id = new ExprAstVariable(m_curToken.id);
      getNextToken();
      TRACE(("}parseIdentifier()\n"));
      return id;
    }

    ExprAst *parseLiteral()
    {
      TRACE(("{parseLiteral(%s)\n",m_curToken.id.data()));
      ExprAst *lit = new ExprAstLiteral(m_curToken.id);
      getNextToken();
      TRACE(("}parseLiteral()\n"));
      return lit;
    }

    ExprAst *parseIdentifierOptionalArgs()
    {
      TRACE(("{parseIdentifierOptionalArgs(%s)\n",m_curToken.id.data()));
      ExprAst *expr = parseIdentifier();
      if (expr)
      {
        if (m_curToken.type==ExprToken::Operator &&
            m_curToken.op==Operator::Colon)
        {
          getNextToken();
          ExprAst *argExpr = parsePrimaryExpression();
          QList<ExprAst> args;
          args.append(argExpr);
          while (m_curToken.type==ExprToken::Operator &&
                 m_curToken.op==Operator::Comma)
          {
            getNextToken();
            argExpr = parsePrimaryExpression();
            args.append(argExpr);
          }
          expr = new ExprAstFunctionVariable(expr,args);
        }
      }
      TRACE(("}parseIdentifierOptionalArgs()\n"));
      return expr;
    }

    ExprAst *parseFilteredVariable()
    {
      TRACE(("{parseFilteredVariable()\n"));
      ExprAst *expr = parseIdentifierOptionalArgs();
      if (expr)
      {
        while (m_curToken.type==ExprToken::Operator &&
               m_curToken.op==Operator::Filter)
        {
          getNextToken();
          ExprAstFilter *filter = parseFilter();
          if (!filter) break;
          expr = new ExprAstFilterAppl(expr,filter);
        }
      }
      TRACE(("}parseFilteredVariable()\n"));
      return expr;
    }

    ExprAstFilter *parseFilter()
    {
      TRACE(("{parseFilter(%s)\n",m_curToken.id.data()));
      QCString filterName = m_curToken.id;
      getNextToken();
      ExprAst *argExpr=0;
      if (m_curToken.type==ExprToken::Operator &&
          m_curToken.op==Operator::Colon)
      {
        getNextToken();
        argExpr = parsePrimaryExpression();
      }
      ExprAstFilter *filter = new ExprAstFilter(filterName,argExpr);
      TRACE(("}parseFilter()\n"));
      return filter;
    }


    bool getNextToken()
    {
      const char *p = m_tokenStream;
      char s[2];
      s[1]=0;
      if (p==0 || *p=='\0') return FALSE;
      while (*p==' ') p++; // skip over spaces
      char c=*p;
      if (strncmp(p,"not ",4)==0)
      {
        m_curToken.type = ExprToken::Operator;
        m_curToken.op = Operator::Not;
        p+=4;
      }
      else if (strncmp(p,"and ",4)==0)
      {
        m_curToken.type = ExprToken::Operator;
        m_curToken.op = Operator::And;
        p+=4;
      }
      else if (strncmp(p,"or ",3)==0)
      {
        m_curToken.type = ExprToken::Operator;
        m_curToken.op = Operator::Or;
        p+=3;
      }
      else if (c=='=' && *(p+1)=='=')
      {
        m_curToken.type = ExprToken::Operator;
        m_curToken.op = Operator::Equal;
        p+=2;
      }
      else if (c=='!' && *(p+1)=='=')
      {
        m_curToken.type = ExprToken::Operator;
        m_curToken.op = Operator::NotEqual;
        p+=2;
      }
      else if (c=='<' && *(p+1)=='=')
      {
        m_curToken.type = ExprToken::Operator;
        m_curToken.op = Operator::LessEqual;
        p+=2;
      }
      else if (c=='>' && *(p+1)=='=')
      {
        m_curToken.type = ExprToken::Operator;
        m_curToken.op = Operator::GreaterEqual;
        p+=2;
      }
      else if (c=='<')
      {
        m_curToken.type = ExprToken::Operator;
        m_curToken.op = Operator::Less;
        p++;
      }
      else if (c=='>')
      {
        m_curToken.type = ExprToken::Operator;
        m_curToken.op = Operator::Greater;
        p++;
      }
      else if (c=='|')
      {
        m_curToken.type = ExprToken::Operator;
        m_curToken.op = Operator::Filter;
        p++;
      }
      else if (c==':')
      {
        m_curToken.type = ExprToken::Operator;
        m_curToken.op = Operator::Colon;
        p++;
      }
      else if (c==',')
      {
        m_curToken.type = ExprToken::Operator;
        m_curToken.op = Operator::Comma;
        p++;
      }
      else if ((c=='-' && *(p+1)>='0' && *(p+1)<='9') || (c>='0' && c<='9'))
      {
        m_curToken.type = ExprToken::Number;
        const char *np = p;
        if (c=='-') np++;
        m_curToken.num = 0;
        while (*np>='0' && *np<='9')
        {
          m_curToken.num*=10;
          m_curToken.num+=*np-'0';
          np++;
        }
        if (c=='-') m_curToken.num=-m_curToken.num;
        p=np;
      }
      else if (c=='_' || (c>='a' && c<='z') || (c>='A' && c<='Z'))
      {
        m_curToken.type = ExprToken::Identifier;
        s[0]=c;
        m_curToken.id = s;
        p++;
        while ((c=*p) &&
            (c=='_' || c=='.' ||
             (c>='a' && c<='z') ||
             (c>='A' && c<='Z') ||
             (c>='0' && c<='9'))
            )
        {
          s[0]=c;
          m_curToken.id+=s;
          p++;
        }
        if (m_curToken.id=="True") // treat true literal as numerical 1
        {
          m_curToken.type = ExprToken::Number;
          m_curToken.num = 1;
        }
        else if (m_curToken.id=="False") // treat false literal as numerical 0
        {
          m_curToken.type = ExprToken::Number;
          m_curToken.num = 0;
        }
      }
      else if (c=='"' || c=='\'')
      {
        m_curToken.type = ExprToken::Literal;
        m_curToken.id.resize(0);
        p++;
        char tokenChar = c;
        char cp=0;
        while ((c=*p) && (c!=tokenChar || (c==tokenChar && cp=='\\')))
        {
          s[0]=c;
          if (c!='\\' || cp=='\\') // don't add escapes
          {
            m_curToken.id+=s;
          }
          cp=c;
          p++;
        }
        if (*p==tokenChar) p++;
      }
      else
      {
        m_curToken.type = ExprToken::Unknown;
        char s[2];
        s[0]=c;
        s[1]=0;
        warn(m_parser->templateName(),m_line,"Found unknown token %s while parsing %s",s,m_tokenStream);
        m_curToken.id = s;
        p++;
      }
      //TRACE(("token type=%d op=%d num=%d id=%s\n",
      //    m_curToken.type,m_curToken.op,m_curToken.num,m_curToken.id.data()));

      m_tokenStream = p;
      return TRUE;
    }

    const TemplateParser *m_parser;
    ExprToken m_curToken;
    int m_line;
    const char *m_tokenStream;
};

//----------------------------------------------------------

/** @brief Class representing a lexical token in a template */
class TemplateToken
{
  public:
    enum Type { Text, Variable, Block };
    TemplateToken(Type t,const char *d,int l) : type(t), data(d), line(l) {}
    Type type;
    QCString data;
    int line;
};

//----------------------------------------------------------

/** @brief Class representing a list of AST nodes in a template */
class TemplateNodeList : public QList<TemplateNode>
{
  public:
    TemplateNodeList()
    {
      setAutoDelete(TRUE);
    }
    void render(FTextStream &ts,TemplateContext *c)
    {
      TRACE(("{TemplateNodeList::render\n"));
      QListIterator<TemplateNode> it(*this);
      TemplateNode *tn=0;
      for (it.toFirst();(tn=it.current());++it)
      {
        tn->render(ts,c);
      }
      TRACE(("}TemplateNodeList::render\n"));
    }
};

//----------------------------------------------------------

/** @brief Internal class representing the implementation of a template */
class TemplateImpl : public TemplateNode, public Template
{
  public:
    TemplateImpl(TemplateEngine *e,const QCString &name,const QCString &data);
    void render(FTextStream &ts, TemplateContext *c);

    TemplateEngine *engine() const { return m_engine; }
    TemplateBlockContext *blockContext() { return &m_blockContext; }

  private:
    TemplateEngine *m_engine;
    QCString m_name;
    TemplateNodeList m_nodes;
    TemplateBlockContext m_blockContext;
};

//----------------------------------------------------------


TemplateContextImpl::TemplateContextImpl(const TemplateEngine *e)
  : m_engine(e), m_templateName("<unknown>"), m_line(1), m_escapeIntf(0),
    m_spacelessIntf(0), m_spacelessEnabled(FALSE)
{
  m_contextStack.setAutoDelete(TRUE);
  push();
}

TemplateContextImpl::~TemplateContextImpl()
{
  pop();
}

void TemplateContextImpl::set(const char *name,const TemplateVariant &v)
{
  TemplateVariant *pv = m_contextStack.getFirst()->find(name);
  if (pv)
  {
    m_contextStack.getFirst()->remove(name);
  }
  m_contextStack.getFirst()->insert(name,new TemplateVariant(v));
}

TemplateVariant TemplateContextImpl::get(const QCString &name) const
{
  int i=name.find('.');
  if (i==-1) // simple name
  {
    return getPrimary(name);
  }
  else // obj.prop
  {
    TemplateVariant v;
    QCString objName = name.left(i);
    v = getPrimary(objName);
    QCString propName = name.mid(i+1);
    while (!propName.isEmpty())
    {
      //printf("getPrimary(%s) type=%d:%s\n",objName.data(),v.type(),v.toString().data());
      if (v.type()==TemplateVariant::Struct)
      {
        i = propName.find(".");
        int l = i==-1 ? propName.length() : i;
        v = v.toStruct()->get(propName.left(l));
        if (!v.isValid())
        {
          warn(m_templateName,m_line,"requesting non-existing property '%s' for object '%s'",propName.left(l).data(),objName.data());
        }
        if (i!=-1)
        {
          objName = propName.left(i);
          propName = propName.mid(i+1);
        }
        else
        {
          propName.resize(0);
        }
      }
      else if (v.type()==TemplateVariant::List)
      {
        i = propName.find(".");
        int l = i==-1 ? propName.length() : i;
        bool b;
        int index = propName.left(l).toInt(&b);
        if (b)
        {
          v = v.toList()->at(index);
        }
        else
        {
          warn(m_templateName,m_line,"list index '%s' is not valid",propName.data());
          break;
        }
        if (i!=-1)
        {
          propName = propName.mid(i+1);
        }
        else
        {
          propName.resize(0);
        }
      }
      else
      {
        warn(m_templateName,m_line,"using . on an object '%s' is not an struct or list",objName.data());
        return TemplateVariant();
      }
    } while (i!=-1);
    return v;
  }
}

const TemplateVariant *TemplateContextImpl::getRef(const QCString &name) const
{
  QListIterator< QDict<TemplateVariant> > it(m_contextStack);
  QDict<TemplateVariant> *dict;
  for (it.toFirst();(dict=it.current());++it)
  {
    TemplateVariant *v = dict->find(name);
    if (v) return v;
  }
  return 0; // not found
}

TemplateVariant TemplateContextImpl::getPrimary(const QCString &name) const
{
  const TemplateVariant *v = getRef(name);
  return v ? *v : TemplateVariant();
}

void TemplateContextImpl::push()
{
  QDict<TemplateVariant> *dict = new QDict<TemplateVariant>;
  dict->setAutoDelete(TRUE);
  m_contextStack.prepend(dict);
}

void TemplateContextImpl::pop()
{
  if (!m_contextStack.removeFirst())
  {
    warn(m_templateName,m_line,"pop() called on empty context stack!\n");
  }
}

TemplateBlockContext *TemplateContextImpl::blockContext()
{
  return &m_blockContext;
}

void TemplateContextImpl::warn(const char *fileName,int line,const char *fmt,...) const
{
  va_list args;
  va_start(args,fmt);
  va_warn(fileName,line,fmt,args);
  va_end(args);
  m_engine->printIncludeContext(fileName,line);
}

//----------------------------------------------------------

/** @brief Class representing a piece of plain text in a template */
class TemplateNodeText : public TemplateNode
{
  public:
    TemplateNodeText(TemplateParser *,TemplateNode *parent,int,const QCString &data)
      : TemplateNode(parent), m_data(data)
    {
      TRACE(("TemplateNodeText('%s')\n",replace(data,'\n',' ').data()));
    }

    void render(FTextStream &ts, TemplateContext *c)
    {
      //printf("TemplateNodeText::render(%s)\n",m_data.data());
      TemplateContextImpl* ci = dynamic_cast<TemplateContextImpl*>(c);
      if (ci->spacelessEnabled())
      {
        ts << ci->spacelessIntf()->remove(m_data);
      }
      else
      {
        ts << m_data;
      }
    }
  private:
    QCString m_data;
};

//----------------------------------------------------------

/** @brief Class representing a variable in a template */
class TemplateNodeVariable : public TemplateNode
{
  public:
    TemplateNodeVariable(TemplateParser *parser,TemplateNode *parent,int line,const QCString &var)
      : TemplateNode(parent), m_templateName(parser->templateName()), m_line(line)
    {
      TRACE(("TemplateNodeVariable(%s)\n",var.data()));
      ExpressionParser expParser(parser,line);
      m_var = expParser.parseVariable(var);
    }
    ~TemplateNodeVariable()
    {
      delete m_var;
    }

    void render(FTextStream &ts, TemplateContext *c)
    {
      TemplateContextImpl* ci = dynamic_cast<TemplateContextImpl*>(c);
      ci->setLocation(m_templateName,m_line);
      TemplateVariant v = m_var->resolve(c);
      if (v.type()==TemplateVariant::Function)
      {
        v = v.call(QValueList<TemplateVariant>());
      }
      //printf("TemplateNodeVariable::render(%s) raw=%d\n",value.data(),v.raw());
      if (ci->escapeIntf() && !v.raw())
      {
        ts << ci->escapeIntf()->escape(v.toString());
      }
      else
      {
        ts << v.toString();
      }
    }

  private:
    QCString m_templateName;
    int m_line;
    ExprAst *m_var;
    QList<ExprAst> m_args;
};

//----------------------------------------------------------

/** @brief Helper class for creating template AST tag nodes and returning
  * the template for a given node.
 */
template<class T> class TemplateNodeCreator : public TemplateNode
{
  public:
    TemplateNodeCreator(TemplateParser *parser,TemplateNode *parent,int line)
      : TemplateNode(parent), m_templateName(parser->templateName()), m_line(line) {}
    static TemplateNode *createInstance(TemplateParser *parser,
                                        TemplateNode *parent,
                                        int line,
                                        const QCString &data)
    {
      return new T(parser,parent,line,data);
    }
    TemplateImpl *getTemplate()
    {
      TemplateNode *root = this;
      while (root && root->parent())
      {
        root = root->parent();
      }
      return dynamic_cast<TemplateImpl*>(root);
    }
  protected:
    QCString m_templateName;
    int m_line;
};

//----------------------------------------------------------

/** @brief Class representing an 'if' tag in a template */
class TemplateNodeIf : public TemplateNodeCreator<TemplateNodeIf>
{
  public:
    TemplateNodeIf(TemplateParser *parser,TemplateNode *parent,int line,const QCString &data) :
      TemplateNodeCreator<TemplateNodeIf>(parser,parent,line)
    {
      TRACE(("{TemplateNodeIf(%s)\n",data.data()));
      if (data.isEmpty())
      {
        parser->warn(m_templateName,line,"missing argument for if tag");
      }
      QStrList stopAt;
      stopAt.append("endif");
      stopAt.append("else");
      parser->parse(this,line,stopAt,m_trueNodes);
      TemplateToken *tok = parser->takeNextToken();
      ExpressionParser ex(parser,line);
      m_guardAst = ex.parse(data);

      if (tok && tok->data=="else")
      {
        stopAt.removeLast();
        parser->parse(this,line,stopAt,m_falseNodes);
        parser->removeNextToken(); // skip over endif
      }
      delete tok;
      TRACE(("}TemplateNodeIf(%s)\n",data.data()));
    }
    ~TemplateNodeIf()
    {
      delete m_guardAst;
    }

    void render(FTextStream &ts, TemplateContext *c)
    {
      TemplateContextImpl* ci = dynamic_cast<TemplateContextImpl*>(c);
      ci->setLocation(m_templateName,m_line);
      //printf("TemplateNodeIf::render #trueNodes=%d #falseNodes=%d\n",m_trueNodes.count(),m_falseNodes.count());
      if (m_guardAst)
      {
        TemplateVariant guardValue = m_guardAst->resolve(c);
        if (guardValue.toBool()) // guard is true, render corresponding nodes
        {
          m_trueNodes.render(ts,c);
        }
        else // guard is false, render corresponding nodes
        {
          m_falseNodes.render(ts,c);
        }
      }
    }
  private:
    ExprAst *m_guardAst;
    TemplateNodeList m_trueNodes;
    TemplateNodeList m_falseNodes;
};

//----------------------------------------------------------
/** @brief Class representing a 'for' tag in a template */
class TemplateNodeRepeat : public TemplateNodeCreator<TemplateNodeRepeat>
{
  public:
    TemplateNodeRepeat(TemplateParser *parser,TemplateNode *parent,int line,const QCString &data)
      : TemplateNodeCreator<TemplateNodeRepeat>(parser,parent,line)
    {
      TRACE(("{TemplateNodeRepeat(%s)\n",data.data()));
      ExpressionParser expParser(parser,line);
      m_expr = expParser.parseVariable(data);
      QStrList stopAt;
      stopAt.append("endrepeat");
      parser->parse(this,line,stopAt,m_repeatNodes);
      parser->removeNextToken(); // skip over endrepeat
      TRACE(("}TemplateNodeRepeat(%s)\n",data.data()));
    }
    ~TemplateNodeRepeat()
    {
      delete m_expr;
    }
    void render(FTextStream &ts, TemplateContext *c)
    {
      TemplateContextImpl* ci = dynamic_cast<TemplateContextImpl*>(c);
      ci->setLocation(m_templateName,m_line);
      TemplateVariant v;
      if (m_expr && (v=m_expr->resolve(c)).type()==TemplateVariant::Integer)
      {
        int i, n = v.toInt();
        for (i=0;i<n;i++)
        {
          TemplateStruct s;
          s.set("counter0",    (int)i);
          s.set("counter",     (int)(i+1));
          s.set("revcounter",  (int)(n-i));
          s.set("revcounter0", (int)(n-i-1));
          s.set("first",i==0);
          s.set("last", i==n-1);
          c->set("repeatloop",&s);
          // render all items for this iteration of the loop
          m_repeatNodes.render(ts,c);
        }
      }
      else // simple type...
      {
        ci->warn(m_templateName,m_line,"for requires a variable of list type!");
      }
    }
  private:
    TemplateNodeList m_repeatNodes;
    ExprAst *m_expr;
};

//----------------------------------------------------------

/** @brief Class representing a 'for' tag in a template */
class TemplateNodeFor : public TemplateNodeCreator<TemplateNodeFor>
{
  public:
    TemplateNodeFor(TemplateParser *parser,TemplateNode *parent,int line,const QCString &data)
      : TemplateNodeCreator<TemplateNodeFor>(parser,parent,line)
    {
      TRACE(("{TemplateNodeFor(%s)\n",data.data()));
      QCString exprStr;
      int i = data.find(" in ");
      if (i==-1)
      {
        if (data.right(3)==" in")
        {
          parser->warn(m_templateName,line,"for is missing container after 'in' keyword");
        }
        else if (data=="in")
        {
          parser->warn(m_templateName,line,"for needs at least one iterator variable");
        }
        else
        {
          parser->warn(m_templateName,line,"for is missing 'in' keyword");
        }
      }
      else
      {
        m_vars = split(data.left(i),",");
        if (m_vars.count()==0)
        {
          parser->warn(m_templateName,line,"for needs at least one iterator variable");
        }

        int j = data.find(" reversed",i);
        m_reversed = (j!=-1);

        if (j==-1) j=data.length();
        if (j>i+4)
        {
          exprStr = data.mid(i+4,j-i-4); // skip over " in " part
        }
        if (exprStr.isEmpty())
        {
          parser->warn(m_templateName,line,"for is missing container after 'in' keyword");
        }
      }
      ExpressionParser expParser(parser,line);
      m_expr = expParser.parseVariable(exprStr);

      QStrList stopAt;
      stopAt.append("endfor");
      stopAt.append("empty");
      parser->parse(this,line,stopAt,m_loopNodes);
      TemplateToken *tok = parser->takeNextToken();
      if (tok && tok->data=="empty")
      {
        stopAt.removeLast();
        parser->parse(this,line,stopAt,m_emptyNodes);
        parser->removeNextToken(); // skip over endfor
      }
      delete tok;
      TRACE(("}TemplateNodeFor(%s)\n",data.data()));
    }

    ~TemplateNodeFor()
    {
      delete m_expr;
    }

    void render(FTextStream &ts, TemplateContext *c)
    {
      TemplateContextImpl* ci = dynamic_cast<TemplateContextImpl*>(c);
      ci->setLocation(m_templateName,m_line);
      //printf("TemplateNodeFor::render #loopNodes=%d #emptyNodes=%d\n",
      //    m_loopNodes.count(),m_emptyNodes.count());
      if (m_expr)
      {
        TemplateVariant v = m_expr->resolve(c);
        const TemplateListIntf *list = v.toList();
        if (list)
        {
          uint listSize = list->count();
          if (listSize==0) // empty for loop
          {
            m_emptyNodes.render(ts,c);
            return;
          }
          c->push();
          //int index = m_reversed ? list.count() : 0;
          TemplateVariant v;
          const TemplateVariant *parentLoop = c->getRef("forloop");
          uint index = m_reversed ? listSize-1 : 0;
          TemplateListIntf::ConstIterator *it = list->createIterator();
          for (m_reversed ? it->toLast() : it->toFirst();
              (it->current(v));
              m_reversed ? it->toPrev() : it->toNext())
          {
            TemplateStruct s;
            s.set("counter0",    (int)index);
            s.set("counter",     (int)(index+1));
            s.set("revcounter",  (int)(listSize-index));
            s.set("revcounter0", (int)(listSize-index-1));
            s.set("first",index==0);
            s.set("last", index==listSize-1);
            s.set("parentloop",parentLoop ? *parentLoop : TemplateVariant());
            c->set("forloop",&s);

            // add variables for this loop to the context
            //obj->addVariableToContext(index,m_vars,c);
            uint vi=0;
            if (m_vars.count()==1) // loop variable represents an item
            {
              c->set(m_vars[vi++],v);
            }
            else if (m_vars.count()>1 && v.type()==TemplateVariant::Struct)
              // loop variables represent elements in a list item
            {
              for (uint i=0;i<m_vars.count();i++,vi++)
              {
                c->set(m_vars[vi],v.toStruct()->get(m_vars[vi]));
              }
            }
            for (;vi<m_vars.count();vi++)
            {
              c->set(m_vars[vi],TemplateVariant());
            }

            // render all items for this iteration of the loop
            m_loopNodes.render(ts,c);

            if (m_reversed) index--; else index++;
          }
          c->pop();
          delete it;
        }
        else // simple type...
        {
          ci->warn(m_templateName,m_line,"for requires a variable of list type!");
        }
      }
    }

  private:
    bool m_reversed;
    ExprAst *m_expr;
    QValueList<QCString> m_vars;
    TemplateNodeList m_loopNodes;
    TemplateNodeList m_emptyNodes;
};

//----------------------------------------------------------

/** @brief Class representing an 'markers' tag in a template */
class TemplateNodeMsg : public TemplateNodeCreator<TemplateNodeMsg>
{
  public:
    TemplateNodeMsg(TemplateParser *parser,TemplateNode *parent,int line,const QCString &)
      : TemplateNodeCreator<TemplateNodeMsg>(parser,parent,line)
    {
      TRACE(("{TemplateNodeMsg()\n"));
      QStrList stopAt;
      stopAt.append("endmsg");
      parser->parse(this,line,stopAt,m_nodes);
      parser->removeNextToken(); // skip over endmarkers
      TRACE(("}TemplateNodeMsg()\n"));
    }
    void render(FTextStream &, TemplateContext *c)
    {
      TemplateContextImpl* ci = dynamic_cast<TemplateContextImpl*>(c);
      ci->setLocation(m_templateName,m_line);
      TemplateEscapeIntf *escIntf = ci->escapeIntf();
      ci->setEscapeIntf(0); // avoid escaping things we send to standard out
      bool enable = ci->spacelessEnabled();
      ci->enableSpaceless(FALSE);
      FTextStream ts(stdout);
      m_nodes.render(ts,c);
      ts << endl;
      ci->setEscapeIntf(escIntf);
      ci->enableSpaceless(enable);
    }
  private:
    TemplateNodeList m_nodes;
};


//----------------------------------------------------------

/** @brief Class representing a 'block' tag in a template */
class TemplateNodeBlock : public TemplateNodeCreator<TemplateNodeBlock>
{
  public:
    TemplateNodeBlock(TemplateParser *parser,TemplateNode *parent,int line,const QCString &data)
      : TemplateNodeCreator<TemplateNodeBlock>(parser,parent,line)
    {
      TRACE(("{TemplateNodeBlock(%s)\n",data.data()));
      m_blockName = data;
      if (m_blockName.isEmpty())
      {
        parser->warn(parser->templateName(),line,"block tag without name");
      }
      QStrList stopAt;
      stopAt.append("endblock");
      parser->parse(this,line,stopAt,m_nodes);
      parser->removeNextToken(); // skip over endblock
      TRACE(("}TemplateNodeBlock(%s)\n",data.data()));
    }

    void render(FTextStream &ts, TemplateContext *c)
    {
      TemplateContextImpl *ci = dynamic_cast<TemplateContextImpl*>(c);
      ci->setLocation(m_templateName,m_line);
      TemplateImpl *t = getTemplate();
      if (t)
      {
        // remove block from the context, so block.super can work
        TemplateNodeBlock *nb = ci->blockContext()->pop(m_blockName);
        if (nb) // block is overruled
        {
          ci->push();
          QGString super;
          FTextStream ss(&super);
          // get super block of block nb
          TemplateNodeBlock *sb = ci->blockContext()->get(m_blockName);
          if (sb && sb!=nb && sb!=this) // nb and sb both overrule this block
          {
            sb->render(ss,c); // render parent of nb to string
          }
          else if (nb!=this) // only nb overrules this block
          {
            m_nodes.render(ss,c); // render parent of nb to string
          }
          // add 'block.super' variable to allow access to parent block content
          TemplateStruct superBlock;
          superBlock.set("super",TemplateVariant(super.data(),TRUE));
          ci->set("block",&superBlock);
          // render the overruled block contents
          t->engine()->enterBlock(nb->m_templateName,nb->m_blockName,nb->m_line);
          nb->m_nodes.render(ts,c);
          t->engine()->leaveBlock();
          ci->pop();
          // re-add block to the context
          ci->blockContext()->push(nb);
        }
        else // block has no overrule
        {
          t->engine()->enterBlock(m_templateName,m_blockName,m_line);
          m_nodes.render(ts,c);
          t->engine()->leaveBlock();
        }
      }
    }

    QCString name() const
    {
      return m_blockName;
    }

  private:
    QCString m_blockName;
    TemplateNodeList m_nodes;
};

//----------------------------------------------------------

/** @brief Class representing a 'extend' tag in a template */
class TemplateNodeExtend : public TemplateNodeCreator<TemplateNodeExtend>
{
  public:
    TemplateNodeExtend(TemplateParser *parser,TemplateNode *parent,int line,const QCString &data)
      : TemplateNodeCreator<TemplateNodeExtend>(parser,parent,line)
    {
      TRACE(("{TemplateNodeExtend(%s)\n",data.data()));
      ExpressionParser ep(parser,line);
      if (data.isEmpty())
      {
        parser->warn(m_templateName,line,"extend tag is missing template file argument");
      }
      m_extendExpr = ep.parsePrimary(data);
      QStrList stopAt;
      parser->parse(this,line,stopAt,m_nodes);
      TRACE(("}TemplateNodeExtend(%s)\n",data.data()));
    }
   ~TemplateNodeExtend()
    {
      delete m_extendExpr;
    }

    void render(FTextStream &ts, TemplateContext *c)
    {
      TemplateContextImpl* ci = dynamic_cast<TemplateContextImpl*>(c);
      ci->setLocation(m_templateName,m_line);
      if (m_extendExpr==0) return;

      QCString extendFile = m_extendExpr->resolve(c).toString();
      if (extendFile.isEmpty())
      {
        ci->warn(m_templateName,m_line,"invalid parameter for extend command");
      }

      // goto root of tree (template node)
      TemplateImpl *t = getTemplate();
      if (t)
      {
        Template *bt = t->engine()->loadByName(extendFile,m_line);
        TemplateImpl *baseTemplate = bt ? dynamic_cast<TemplateImpl*>(bt) : 0;
        if (baseTemplate)
        {
          // fill block context
          TemplateContextImpl *ci = dynamic_cast<TemplateContextImpl*>(c);
          TemplateBlockContext *bc = ci->blockContext();

          // add overruling blocks to the context
          QListIterator<TemplateNode> li(m_nodes);
          TemplateNode *n;
          for (li.toFirst();(n=li.current());++li)
          {
            TemplateNodeBlock *nb = dynamic_cast<TemplateNodeBlock*>(n);
            if (nb)
            {
              bc->add(nb);
            }
            TemplateNodeMsg *msg = dynamic_cast<TemplateNodeMsg*>(n);
            if (msg)
            {
              msg->render(ts,c);
            }
          }

          // render the base template with the given context
          baseTemplate->render(ts,c);

          // clean up
          bc->clear();
          t->engine()->unload(t);
        }
        else
        {
          ci->warn(m_templateName,m_line,"failed to load template %s for extend",extendFile.data());
        }
      }
    }

  private:
    ExprAst *m_extendExpr;
    TemplateNodeList m_nodes;
};

/** @brief Class representing an 'include' tag in a template */
class TemplateNodeInclude : public TemplateNodeCreator<TemplateNodeInclude>
{
  public:
    TemplateNodeInclude(TemplateParser *parser,TemplateNode *parent,int line,const QCString &data)
      : TemplateNodeCreator<TemplateNodeInclude>(parser,parent,line)
    {
      TRACE(("TemplateNodeInclude(%s)\n",data.data()));
      ExpressionParser ep(parser,line);
      if (data.isEmpty())
      {
        parser->warn(m_templateName,line,"include tag is missing template file argument");
      }
      m_includeExpr = ep.parsePrimary(data);
    }
   ~TemplateNodeInclude()
    {
      delete m_includeExpr;
    }
    void render(FTextStream &ts, TemplateContext *c)
    {
      TemplateContextImpl* ci = dynamic_cast<TemplateContextImpl*>(c);
      ci->setLocation(m_templateName,m_line);
      if (m_includeExpr)
      {
        QCString includeFile = m_includeExpr->resolve(c).toString();
        if (includeFile.isEmpty())
        {
          ci->warn(m_templateName,m_line,"invalid parameter for include command\n");
        }
        else
        {
          TemplateImpl *t = getTemplate();
          if (t)
          {
            Template *it = t->engine()->loadByName(includeFile,m_line);
            TemplateImpl *incTemplate = it ? dynamic_cast<TemplateImpl*>(it) : 0;
            if (incTemplate)
            {
              incTemplate->render(ts,c);
              t->engine()->unload(t);
            }
            else
            {
              ci->warn(m_templateName,m_line,"failed to load template '%s' for include",includeFile.data()?includeFile.data():"");
            }
          }
        }
      }
    }

  private:
    ExprAst *m_includeExpr;
};

//----------------------------------------------------------

/** @brief Class representing an 'create' tag in a template */
class TemplateNodeCreate : public TemplateNodeCreator<TemplateNodeCreate>
{
  public:
    TemplateNodeCreate(TemplateParser *parser,TemplateNode *parent,int line,const QCString &data)
      : TemplateNodeCreator<TemplateNodeCreate>(parser,parent,line)
    {
      TRACE(("TemplateNodeCreate(%s)\n",data.data()));
      ExpressionParser ep(parser,line);
      if (data.isEmpty())
      {
        parser->warn(m_templateName,line,"create tag is missing arguments");
      }
      int i = data.find(" from ");
      if (i==-1)
      {
        if (data.right(3)==" from")
        {
          parser->warn(m_templateName,line,"create is missing template name after 'from' keyword");
        }
        else if (data=="from")
        {
          parser->warn(m_templateName,line,"create needs a file name and a template name");
        }
        else
        {
          parser->warn(m_templateName,line,"create is missing 'from' keyword");
        }
      }
      else
      {
        ExpressionParser ep(parser,line);
        m_fileExpr = ep.parsePrimary(data.left(i).stripWhiteSpace());
        m_templateExpr = ep.parsePrimary(data.mid(i+6).stripWhiteSpace());
      }
    }
   ~TemplateNodeCreate()
    {
      delete m_templateExpr;
      delete m_fileExpr;
    }
    void render(FTextStream &, TemplateContext *c)
    {
      TemplateContextImpl* ci = dynamic_cast<TemplateContextImpl*>(c);
      ci->setLocation(m_templateName,m_line);
      if (m_templateExpr && m_fileExpr)
      {
        QCString templateFile = m_templateExpr->resolve(c).toString();
        QCString outputFile = m_fileExpr->resolve(c).toString();
        if (templateFile.isEmpty())
        {
          ci->warn(m_templateName,m_line,"empty template name parameter for create command\n");
        }
        else if (outputFile.isEmpty())
        {
          ci->warn(m_templateName,m_line,"empty file name parameter for create command\n");
        }
        else
        {
          TemplateImpl *t = getTemplate();
          if (t)
          {
            Template *ct = t->engine()->loadByName(templateFile,m_line);
            TemplateImpl *createTemplate = ct ? dynamic_cast<TemplateImpl*>(ct) : 0;
            if (createTemplate)
            {
              if (!ci->outputDirectory().isEmpty())
              {
                outputFile.prepend(ci->outputDirectory()+"/");
              }
              QFile f(outputFile);
              if (f.open(IO_WriteOnly))
              {
                FTextStream ts(&f);
                createTemplate->render(ts,c);
                t->engine()->unload(t);
              }
              else
              {
                ci->warn(m_templateName,m_line,"failed to open output file '%s' for create command",outputFile.data());
              }
            }
            else
            {
              ci->warn(m_templateName,m_line,"failed to load template '%s' for include",templateFile.data());
            }
          }
        }
      }
    }

  private:
    ExprAst *m_templateExpr;
    ExprAst *m_fileExpr;
};

//----------------------------------------------------------

/** @brief Class representing an 'tree' tag in a template */
class TemplateNodeTree : public TemplateNodeCreator<TemplateNodeTree>
{
    struct TreeContext
    {
      TreeContext(TemplateNodeTree *o,const TemplateListIntf *l,TemplateContext *c)
        : object(o), list(l), templateCtx(c) {}
      TemplateNodeTree *object;
      const TemplateListIntf *list;
      TemplateContext  *templateCtx;
    };
  public:
    TemplateNodeTree(TemplateParser *parser,TemplateNode *parent,int line,const QCString &data)
      : TemplateNodeCreator<TemplateNodeTree>(parser,parent,line)
    {
      TRACE(("{TemplateNodeTree(%s)\n",data.data()));
      ExpressionParser ep(parser,line);
      if (data.isEmpty())
      {
        parser->warn(m_templateName,line,"recursetree tag is missing data argument");
      }
      m_treeExpr = ep.parsePrimary(data);
      QStrList stopAt;
      stopAt.append("endrecursetree");
      parser->parse(this,line,stopAt,m_treeNodes);
      parser->removeNextToken(); // skip over endrecursetree
      TRACE(("}TemplateNodeTree(%s)\n",data.data()));
    }
    ~TemplateNodeTree()
    {
      delete m_treeExpr;
    }
    static TemplateVariant renderChildrenStub(const void *ctx, const QValueList<TemplateVariant> &)
    {
      return TemplateVariant(((TreeContext*)ctx)->object->
                             renderChildren((const TreeContext*)ctx),TRUE);
    }
    QCString renderChildren(const TreeContext *ctx)
    {
      //printf("TemplateNodeTree::renderChildren(%d)\n",ctx->list->count());
      // render all children of node to a string and return it
      QGString result;
      FTextStream ss(&result);
      TemplateContext *c = ctx->templateCtx;
      c->push();
      TemplateVariant node;
      TemplateListIntf::ConstIterator *it = ctx->list->createIterator();
      for (it->toFirst();(it->current(node));it->toNext())
      {
        c->set("node",node);
        bool hasChildren=FALSE;
        const TemplateStructIntf *ns = node.toStruct();
        if (ns) // node is a struct
        {
          TemplateVariant v = ns->get("children");
          if (v.isValid()) // with a field 'children'
          {
            const TemplateListIntf *list = v.toList();
            if (list && list->count()>0) // non-empty list
            {
              TreeContext childCtx(this,list,ctx->templateCtx);
//              TemplateVariant children(&childCtx,renderChildrenStub);
              TemplateVariant children(TemplateVariant::Delegate::fromFunction(&childCtx,renderChildrenStub));
              children.setRaw(TRUE);
              c->set("children",children);
              m_treeNodes.render(ss,c);
              hasChildren=TRUE;
            }
          }
        }
        if (!hasChildren)
        {
          c->set("children",TemplateVariant("")); // provide default
          m_treeNodes.render(ss,c);
        }
      }
      c->pop();
      return result.data();
    }
    void render(FTextStream &ts, TemplateContext *c)
    {
      //printf("TemplateNodeTree::render()\n");
      TemplateContextImpl* ci = dynamic_cast<TemplateContextImpl*>(c);
      ci->setLocation(m_templateName,m_line);
      TemplateVariant v = m_treeExpr->resolve(c);
      const TemplateListIntf *list = v.toList();
      if (list)
      {
        TreeContext ctx(this,list,c);
        ts << renderChildren(&ctx);
      }
      else
      {
        ci->warn(m_templateName,m_line,"recursetree's argument should be a list type");
      }
    }

  private:
    ExprAst         *m_treeExpr;
    TemplateNodeList m_treeNodes;
};

//----------------------------------------------------------

/** @brief Class representing an 'with' tag in a template */
class TemplateNodeWith : public TemplateNodeCreator<TemplateNodeWith>
{
    struct Mapping
    {
      Mapping(const QCString &n,ExprAst *e) : name(n), value(e) {}
     ~Mapping() { delete value; }
      QCString name;
      ExprAst *value;
    };
  public:
    TemplateNodeWith(TemplateParser *parser,TemplateNode *parent,int line,const QCString &data)
      : TemplateNodeCreator<TemplateNodeWith>(parser,parent,line)
    {
      TRACE(("{TemplateNodeWith(%s)\n",data.data()));
      m_args.setAutoDelete(TRUE);
      ExpressionParser expParser(parser,line);
      QValueList<QCString> args = split(data," ");
      QValueListIterator<QCString> it = args.begin();
      while (it!=args.end())
      {
        QCString arg = *it;
        int j=arg.find('=');
        if (j>0)
        {
          ExprAst *expr = expParser.parsePrimary(arg.mid(j+1));
          if (expr)
          {
            m_args.append(new Mapping(arg.left(j),expr));
          }
        }
        else
        {
          parser->warn(parser->templateName(),line,"invalid argument '%s' for with tag",arg.data());
        }
        ++it;
      }
      QStrList stopAt;
      stopAt.append("endwith");
      parser->parse(this,line,stopAt,m_nodes);
      parser->removeNextToken(); // skip over endwith
      TRACE(("}TemplateNodeWith(%s)\n",data.data()));
    }
    ~TemplateNodeWith()
    {
    }
    void render(FTextStream &ts, TemplateContext *c)
    {
      TemplateContextImpl *ci = dynamic_cast<TemplateContextImpl*>(c);
      ci->setLocation(m_templateName,m_line);
      c->push();
      QListIterator<Mapping> it(m_args);
      Mapping *mapping;
      for (it.toFirst();(mapping=it.current());++it)
      {
        TemplateVariant value = mapping->value->resolve(c);
        ci->set(mapping->name,value);
      }
      m_nodes.render(ts,c);
      c->pop();
    }
  private:
    TemplateNodeList m_nodes;
    QList<Mapping> m_args;
};

//----------------------------------------------------------

/** @brief Class representing an 'set' tag in a template */
class TemplateNodeCycle : public TemplateNodeCreator<TemplateNodeCycle>
{
  public:
    TemplateNodeCycle(TemplateParser *parser,TemplateNode *parent,int line,const QCString &data)
      : TemplateNodeCreator<TemplateNodeCycle>(parser,parent,line)
    {
      TRACE(("{TemplateNodeCycle(%s)\n",data.data()));
      m_args.setAutoDelete(TRUE);
      m_index=0;
      ExpressionParser expParser(parser,line);
      QValueList<QCString> args = split(data," ");
      QValueListIterator<QCString> it = args.begin();
      while (it!=args.end())
      {
        ExprAst *expr = expParser.parsePrimary(*it);
        if (expr)
        {
          m_args.append(expr);
        }
        ++it;
      }
      if (m_args.count()<2)
      {
          parser->warn(parser->templateName(),line,"expected at least two arguments for cycle command, got %d",m_args.count());
      }
      TRACE(("}TemplateNodeCycle(%s)\n",data.data()));
    }
    void render(FTextStream &ts, TemplateContext *c)
    {
      TemplateContextImpl *ci = dynamic_cast<TemplateContextImpl*>(c);
      ci->setLocation(m_templateName,m_line);
      if (m_index<m_args.count())
      {
        TemplateVariant v = m_args.at(m_index)->resolve(c);
        if (v.type()==TemplateVariant::Function)
        {
          v = v.call(QValueList<TemplateVariant>());
        }
        if (ci->escapeIntf() && !v.raw())
        {
          ts << ci->escapeIntf()->escape(v.toString());
        }
        else
        {
          ts << v.toString();
        }
      }
      if (++m_index==m_args.count()) // wrap around
      {
        m_index=0;
      }
    }
  private:
    uint m_index;
    QList<ExprAst> m_args;
};

//----------------------------------------------------------

/** @brief Class representing an 'set' tag in a template */
class TemplateNodeSet : public TemplateNodeCreator<TemplateNodeSet>
{
    struct Mapping
    {
      Mapping(const QCString &n,ExprAst *e) : name(n), value(e) {}
     ~Mapping() { delete value; }
      QCString name;
      ExprAst *value;
    };
  public:
    TemplateNodeSet(TemplateParser *parser,TemplateNode *parent,int line,const QCString &data)
      : TemplateNodeCreator<TemplateNodeSet>(parser,parent,line)
    {
      TRACE(("{TemplateNodeSet(%s)\n",data.data()));
      m_args.setAutoDelete(TRUE);
      ExpressionParser expParser(parser,line);
      QValueList<QCString> args = split(data," ");
      QValueListIterator<QCString> it = args.begin();
      while (it!=args.end())
      {
        QCString arg = *it;
        int j=arg.find('=');
        if (j>0)
        {
          ExprAst *expr = expParser.parsePrimary(arg.mid(j+1));
          if (expr)
          {
            m_args.append(new Mapping(arg.left(j),expr));
          }
        }
        else
        {
          parser->warn(parser->templateName(),line,"invalid argument '%s' for with tag",arg.data());
        }
        ++it;
      }
      TRACE(("}TemplateNodeSet(%s)\n",data.data()));
    }
    void render(FTextStream &, TemplateContext *c)
    {
      TemplateContextImpl *ci = dynamic_cast<TemplateContextImpl*>(c);
      ci->setLocation(m_templateName,m_line);
      QListIterator<Mapping> it(m_args);
      Mapping *mapping;
      for (it.toFirst();(mapping=it.current());++it)
      {
        TemplateVariant value = mapping->value->resolve(c);
        ci->set(mapping->name,value);
      }
    }
  private:
    QList<Mapping> m_args;
};

//----------------------------------------------------------

/** @brief Class representing an 'spaceless' tag in a template */
class TemplateNodeSpaceless : public TemplateNodeCreator<TemplateNodeSpaceless>
{
  public:
    TemplateNodeSpaceless(TemplateParser *parser,TemplateNode *parent,int line,const QCString &)
      : TemplateNodeCreator<TemplateNodeSpaceless>(parser,parent,line)
    {
      TRACE(("{TemplateNodeSpaceless()\n"));
      QStrList stopAt;
      stopAt.append("endspaceless");
      parser->parse(this,line,stopAt,m_nodes);
      parser->removeNextToken(); // skip over endwith
      TRACE(("}TemplateNodeSpaceless()\n"));
    }
    void render(FTextStream &ts, TemplateContext *c)
    {
      TemplateContextImpl *ci = dynamic_cast<TemplateContextImpl*>(c);
      ci->setLocation(m_templateName,m_line);
      bool wasSpaceless = ci->spacelessEnabled();
      ci->enableSpaceless(TRUE);
      m_nodes.render(ts,c);
      ci->enableSpaceless(wasSpaceless);
    }
  private:
    TemplateNodeList m_nodes;
};

//----------------------------------------------------------

/** @brief Class representing an 'markers' tag in a template */
class TemplateNodeMarkers : public TemplateNodeCreator<TemplateNodeMarkers>
{
  public:
    TemplateNodeMarkers(TemplateParser *parser,TemplateNode *parent,int line,const QCString &data)
      : TemplateNodeCreator<TemplateNodeMarkers>(parser,parent,line)
    {
      TRACE(("{TemplateNodeMarkers(%s)\n",data.data()));
      int i = data.find(" in ");
      int w = data.find(" with ");
      if (i==-1 || w==-1 || w<i)
      {
        parser->warn(m_templateName,line,"markers tag as wrong format. Expected: markers <var> in <list> with <string_with_markers>");
      }
      else
      {
        ExpressionParser expParser(parser,line);
        m_var = data.left(i);
        m_listExpr = expParser.parseVariable(data.mid(i+4,w-i-4));
        m_patternExpr = expParser.parseVariable(data.right(data.length()-w-6));
      }
      QStrList stopAt;
      stopAt.append("endmarkers");
      parser->parse(this,line,stopAt,m_nodes);
      parser->removeNextToken(); // skip over endmarkers
      TRACE(("}TemplateNodeMarkers(%s)\n",data.data()));
    }
    void render(FTextStream &ts, TemplateContext *c)
    {
      TemplateContextImpl *ci = dynamic_cast<TemplateContextImpl*>(c);
      ci->setLocation(m_templateName,m_line);
      if (!m_var.isEmpty() && m_listExpr && m_patternExpr)
      {
        TemplateVariant v = m_listExpr->resolve(c);
        const TemplateListIntf *list = v.toList();
        TemplateVariant patternStr = m_patternExpr->resolve(c);
        if (list)
        {
          if (patternStr.type()==TemplateVariant::String)
          {
            TemplateListIntf::ConstIterator *it = list->createIterator();
            c->push();
            QCString str = patternStr.toString();
            QRegExp marker("@[0-9]+"); // pattern for a marker, i.e. @0, @1 ... @12, etc
            int index=0,newIndex,matchLen;
            while ((newIndex=marker.match(str,index,&matchLen))!=-1)
            {
              ts << str.mid(index,newIndex-index); // write text before marker
              bool ok;
              uint entryIndex = str.mid(newIndex+1,matchLen-1).toUInt(&ok); // get marker id
              TemplateVariant var;
              uint i=0;
              // search for list element at position id
              for (it->toFirst(); (it->current(var)) && i<entryIndex; it->toNext(),i++) {}
              if (ok && i==entryIndex) // found element
              {
                TemplateStruct s;
                s.set("id",(int)i);
                c->set("markers",&s);
                c->set(m_var,var); // define local variable to hold element of list type
                bool wasSpaceless = ci->spacelessEnabled();
                ci->enableSpaceless(TRUE);
                m_nodes.render(ts,c);
                ci->enableSpaceless(wasSpaceless);
              }
              else if (!ok)
              {
                ci->warn(m_templateName,m_line,"markers pattern string has invalid markers '%s'",str.data());
              }
              else if (i<entryIndex)
              {
                ci->warn(m_templateName,m_line,"markers list does not an element for marker position %d",i);
              }
              index=newIndex+matchLen; // set index just after marker
            }
            ts << str.right(str.length()-index); // write text after last marker
            c->pop();
          }
          else
          {
            ci->warn(m_templateName,m_line,"markers requires a parameter of string type after 'with'!");
          }
        }
        else
        {
          ci->warn(m_templateName,m_line,"markers requires a parameter of list type after 'in'!");
        }
      }
    }
  private:
    TemplateNodeList m_nodes;
    QCString m_var;
    ExprAst *m_listExpr;
    ExprAst *m_patternExpr;
};

//----------------------------------------------------------

/** @brief Factory class for creating tag AST nodes found in a template */
class TemplateNodeFactory
{
  public:
    typedef TemplateNode *(*CreateFunc)(TemplateParser *parser,
                                        TemplateNode *parent,
                                        int line,
                                        const QCString &data);

    static TemplateNodeFactory *instance()
    {
      static TemplateNodeFactory *instance = 0;
      if (instance==0) instance = new TemplateNodeFactory;
      return instance;
    }

    TemplateNode *create(const QCString &name,
                         TemplateParser *parser,
                         TemplateNode *parent,
                         int line,
                         const QCString &data)
    {
      if (m_registry.find(name)==0) return 0;
      return ((CreateFunc)m_registry[name])(parser,parent,line,data);
    }

    void registerTemplateNode(const QCString &name,CreateFunc func)
    {
      m_registry.insert(name,(void*)func);
    }

    /** @brief Helper class for registering a template AST node */
    template<class T> class AutoRegister
    {
      public:
        AutoRegister<T>(const QCString &key)
        {
          TemplateNodeFactory::instance()->registerTemplateNode(key,T::createInstance);
        }
    };

  private:
    QDict<void> m_registry;
};

// register a handler for each start tag we support
static TemplateNodeFactory::AutoRegister<TemplateNodeIf>        autoRefIf("if");
static TemplateNodeFactory::AutoRegister<TemplateNodeFor>       autoRefFor("for");
static TemplateNodeFactory::AutoRegister<TemplateNodeMsg>       autoRefMsg("msg");
static TemplateNodeFactory::AutoRegister<TemplateNodeSet>       autoRefSet("set");
static TemplateNodeFactory::AutoRegister<TemplateNodeTree>      autoRefTree("recursetree");
static TemplateNodeFactory::AutoRegister<TemplateNodeWith>      autoRefWith("with");
static TemplateNodeFactory::AutoRegister<TemplateNodeBlock>     autoRefBlock("block");
static TemplateNodeFactory::AutoRegister<TemplateNodeCycle>     autoRefCycle("cycle");
static TemplateNodeFactory::AutoRegister<TemplateNodeExtend>    autoRefExtend("extend");
static TemplateNodeFactory::AutoRegister<TemplateNodeCreate>    autoRefCreate("create");
static TemplateNodeFactory::AutoRegister<TemplateNodeRepeat>    autoRefRepeat("repeat");
static TemplateNodeFactory::AutoRegister<TemplateNodeInclude>   autoRefInclude("include");
static TemplateNodeFactory::AutoRegister<TemplateNodeMarkers>   autoRefMarkers("markers");
static TemplateNodeFactory::AutoRegister<TemplateNodeSpaceless> autoRefSpaceless("spaceless");

//----------------------------------------------------------

TemplateBlockContext::TemplateBlockContext() : m_blocks(257)
{
  m_blocks.setAutoDelete(TRUE);
}

TemplateNodeBlock *TemplateBlockContext::get(const QCString &name) const
{
  QList<TemplateNodeBlock> *list = m_blocks.find(name);
  if (list==0 || list->count()==0)
  {
    return 0;
  }
  else
  {
    return list->getLast();
  }
}

TemplateNodeBlock *TemplateBlockContext::pop(const QCString &name) const
{
  QList<TemplateNodeBlock> *list = m_blocks.find(name);
  if (list==0 || list->count()==0)
  {
    return 0;
  }
  else
  {
    return list->take(list->count()-1);
  }
}

void TemplateBlockContext::add(TemplateNodeBlock *block)
{
  QList<TemplateNodeBlock> *list = m_blocks.find(block->name());
  if (list==0)
  {
    list = new QList<TemplateNodeBlock>;
    m_blocks.insert(block->name(),list);
  }
  list->prepend(block);
}

void TemplateBlockContext::add(TemplateBlockContext *ctx)
{
  QDictIterator< QList<TemplateNodeBlock> > di(ctx->m_blocks);
  QList<TemplateNodeBlock> *list;
  for (di.toFirst();(list=di.current());++di)
  {
    QListIterator<TemplateNodeBlock> li(*list);
    TemplateNodeBlock *nb;
    for (li.toFirst();(nb=li.current());++li)
    {
      add(nb);
    }
  }
}

void TemplateBlockContext::clear()
{
  m_blocks.clear();
}

void TemplateBlockContext::push(TemplateNodeBlock *block)
{
  QList<TemplateNodeBlock> *list = m_blocks.find(block->name());
  if (list==0)
  {
    list = new QList<TemplateNodeBlock>;
    m_blocks.insert(block->name(),list);
  }
  list->append(block);
}


//----------------------------------------------------------

/** @brief Lexer class for turning a template into a list of tokens */
class TemplateLexer
{
  public:
    TemplateLexer(const TemplateEngine *engine,const QCString &fileName,const QCString &data);
    void tokenize(QList<TemplateToken> &tokens);
  private:
    void addToken(QList<TemplateToken> &tokens,
                  const char *data,int line,int startPos,int endPos,
                  TemplateToken::Type type);
    void reset();
    const TemplateEngine *m_engine;
    QCString m_fileName;
    QCString m_data;
};

TemplateLexer::TemplateLexer(const TemplateEngine *engine,const QCString &fileName,const QCString &data) :
  m_engine(engine), m_fileName(fileName), m_data(data)
{
}

void TemplateLexer::tokenize(QList<TemplateToken> &tokens)
{
  enum LexerStates
  {
    StateText,
    StateBeginTemplate,
    StateTag,
    StateEndTag,
    StateComment,
    StateEndComment,
    StateMaybeVar,
    StateVariable,
    StateEndVariable
  };

  const char *p=m_data.data();
  int  state=StateText;
  int  pos=0;
  int  lastTokenPos=0;
  int  startLinePos=0;
  bool emptyOutputLine=TRUE;
  int  line=1;
  char c;
  int  markStartPos=-1;
  for (;(c=*p);p++,pos++)
  {
    switch (state)
    {
      case StateText:
        if (c=='{') // {{ or {% or {# or something else
        {
          state=StateBeginTemplate;
        }
        else if (c!=' ' && c!='\t' && c!='\n') // non-whitepace text
        {
          emptyOutputLine=FALSE;
        }
        break;
      case StateBeginTemplate:
        switch (c)
        {
          case '%': // {%
            state=StateTag;
            markStartPos=pos-1;
            break;
          case '#': // {#
            state=StateComment;
            markStartPos=pos-1;
            break;
          case '{': // {{
            state=StateMaybeVar;
            markStartPos=pos-1;
            break;
          default:
            state=StateText;
            emptyOutputLine=FALSE;
            break;
        }
        break;
      case StateTag:
        if (c=='\n')
        {
          warn(m_fileName,line,"unexpected new line inside {%%...%%} block");
          m_engine->printIncludeContext(m_fileName,line);
        }
        else if (c=='%') // %} or something else
        {
          state=StateEndTag;
        }
        break;
      case StateEndTag:
        if (c=='}') // %}
        {
          // found tag!
          state=StateText;
          addToken(tokens,m_data.data(),line,lastTokenPos,
                   emptyOutputLine ? startLinePos : markStartPos,
                   TemplateToken::Text);
          addToken(tokens,m_data.data(),line,markStartPos+2,
                   pos-1,TemplateToken::Block);
          lastTokenPos = pos+1;
        }
        else // something else
        {
          if (c=='\n')
          {
            warn(m_fileName,line,"unexpected new line inside {%%...%%} block");
            m_engine->printIncludeContext(m_fileName,line);
          }
          state=StateTag;
        }
        break;
      case StateComment:
        if (c=='\n')
        {
          warn(m_fileName,line,"unexpected new line inside {#...#} block");
          m_engine->printIncludeContext(m_fileName,line);
        }
        else if (c=='#') // #} or something else
        {
          state=StateEndComment;
        }
        break;
      case StateEndComment:
        if (c=='}') // #}
        {
          // found comment tag!
          state=StateText;
          addToken(tokens,m_data.data(),line,lastTokenPos,
                   emptyOutputLine ? startLinePos : markStartPos,
                   TemplateToken::Text);
          lastTokenPos = pos+1;
        }
        else // something else
        {
          if (c=='\n')
          {
            warn(m_fileName,line,"unexpected new line inside {#...#} block");
            m_engine->printIncludeContext(m_fileName,line);
          }
          state=StateComment;
        }
        break;
      case StateMaybeVar:
        switch (c)
        {
          case '#': // {{#
            state=StateComment;
            markStartPos=pos-1;
            break;
          case '%': // {{%
            state=StateTag;
            markStartPos=pos-1;
            break;
          default:  // {{
            state=StateVariable;
            break;
        }
        break;
      case StateVariable:
        if (c=='\n')
        {
          warn(m_fileName,line,"unexpected new line inside {{...}} block");
          m_engine->printIncludeContext(m_fileName,line);
        }
        else if (c=='}') // }} or something else
        {
          state=StateEndVariable;
        }
        break;
      case StateEndVariable:
        if (c=='}') // }}
        {
          // found variable tag!
          state=StateText;
          addToken(tokens,m_data.data(),line,lastTokenPos,
                   emptyOutputLine ? startLinePos : markStartPos,
                   TemplateToken::Text);
          addToken(tokens,m_data.data(),line,markStartPos+2,
                   pos-1,TemplateToken::Variable);
          lastTokenPos = pos+1;
        }
        else // something else
        {
          if (c=='\n')
          {
            warn(m_fileName,line,"unexpected new line inside {{...}} block");
            m_engine->printIncludeContext(m_fileName,line);
          }
          state=StateVariable;
        }
        break;
    }
    if (c=='\n') // new line
    {
      state=StateText;
      startLinePos=pos+1;
      // if the current line only contain commands and whitespace,
      // then skip it in the output by moving lastTokenPos
      if (markStartPos!=-1 && emptyOutputLine) lastTokenPos = startLinePos;
      // reset markers
      markStartPos=-1;
      line++;
      emptyOutputLine=TRUE;
    }
  }
  if (lastTokenPos<pos)
  {
    addToken(tokens,m_data.data(),line,
             lastTokenPos,pos,
             TemplateToken::Text);
  }
}

void TemplateLexer::addToken(QList<TemplateToken> &tokens,
                             const char *data,int line,
                             int startPos,int endPos,
                             TemplateToken::Type type)
{
  if (startPos<endPos)
  {
    int len = endPos-startPos+1;
    QCString text(len+1);
    qstrncpy(text.data(),data+startPos,len);
    text[len]='\0';
    if (type!=TemplateToken::Text) text = text.stripWhiteSpace();
    tokens.append(new TemplateToken(type,text,line));
  }
}

//----------------------------------------------------------

TemplateParser::TemplateParser(const TemplateEngine *engine,
                               const QCString &templateName,
                               QList<TemplateToken> &tokens) :
   m_engine(engine), m_templateName(templateName), m_tokens(tokens)
{
}

void TemplateParser::parse(
                     TemplateNode *parent,int line,const QStrList &stopAt,
                     QList<TemplateNode> &nodes)
{
  TRACE(("{TemplateParser::parse\n"));
  // process the tokens. Build node list
  while (hasNextToken())
  {
    TemplateToken *tok = takeNextToken();
    //printf("%p:Token type=%d data='%s' line=%d\n",
    //       parent,tok->type,tok->data.data(),tok->line);
    switch(tok->type)
    {
      case TemplateToken::Text:
        nodes.append(new TemplateNodeText(this,parent,tok->line,tok->data));
        break;
      case TemplateToken::Variable: // {{ var }}
        nodes.append(new TemplateNodeVariable(this,parent,tok->line,tok->data));
        break;
      case TemplateToken::Block:    // {% tag %}
        {
          QCString command = tok->data;
          int sep = command.find(' ');
          if (sep!=-1)
          {
            command=command.left(sep);
          }
          if (stopAt.contains(command))
          {
            prependToken(tok);
            TRACE(("}TemplateParser::parse: stop\n"));
            return;
          }
          QCString arg;
          if (sep!=-1)
          {
            arg = tok->data.mid(sep+1);
          }
          TemplateNode *node = TemplateNodeFactory::instance()->
                               create(command,this,parent,tok->line,arg);
          if (node)
          {
            nodes.append(node);
          }
          else if (command=="empty"          || command=="else"         ||
                   command=="endif"          || command=="endfor"       ||
                   command=="endblock"       || command=="endwith"      ||
                   command=="endrecursetree" || command=="endspaceless" ||
                   command=="endmarkers"     || command=="endmsg"       ||
                   command=="endrepeat")
          {
            warn(m_templateName,tok->line,"Found tag '%s' without matching start tag",command.data());
          }
          else
          {
            warn(m_templateName,tok->line,"Unknown tag '%s'",command.data());
          }
        }
        break;
    }
    delete tok;
  }
  if (!stopAt.isEmpty())
  {
    QStrListIterator it(stopAt);
    const char *s;
    QCString options;
    for (it.toFirst();(s=it.current());++it)
    {
      if (!options.isEmpty()) options+=", ";
      options+=s;
    }
    warn(m_templateName,line,"Unclosed tag in template, expected one of: %s",
        options.data());
  }
  TRACE(("}TemplateParser::parse: last token\n"));
}

bool TemplateParser::hasNextToken() const
{
  return !m_tokens.isEmpty();
}

TemplateToken *TemplateParser::takeNextToken()
{
  return m_tokens.take(0);
}

const TemplateToken *TemplateParser::currentToken() const
{
  return m_tokens.getFirst();
};

void TemplateParser::removeNextToken()
{
  m_tokens.removeFirst();
}

void TemplateParser::prependToken(const TemplateToken *token)
{
  m_tokens.prepend(token);
}

void TemplateParser::warn(const char *fileName,int line,const char *fmt,...) const
{
  va_list args;
  va_start(args,fmt);
  va_warn(fileName,line,fmt,args);
  va_end(args);
  m_engine->printIncludeContext(fileName,line);
}



//----------------------------------------------------------


TemplateImpl::TemplateImpl(TemplateEngine *engine,const QCString &name,const QCString &data)
  : TemplateNode(0)
{
  m_name = name;
  m_engine = engine;
  TemplateLexer lexer(engine,name,data);
  QList<TemplateToken> tokens;
  tokens.setAutoDelete(TRUE);
  lexer.tokenize(tokens);
  TemplateParser parser(engine,name,tokens);
  parser.parse(this,1,QStrList(),m_nodes);
}

void TemplateImpl::render(FTextStream &ts, TemplateContext *c)
{
  if (!m_nodes.isEmpty())
  {
    TemplateNodeExtend *ne = dynamic_cast<TemplateNodeExtend*>(m_nodes.getFirst());
    if (ne==0) // normal template, add blocks to block context
    {
      TemplateContextImpl *ci = dynamic_cast<TemplateContextImpl*>(c);
      TemplateBlockContext *bc = ci->blockContext();
      QListIterator<TemplateNode> li(m_nodes);
      TemplateNode *n;
      for (li.toFirst();(n=li.current());++li)
      {
        TemplateNodeBlock *nb = dynamic_cast<TemplateNodeBlock*>(n);
        if (nb)
        {
          bc->add(nb);
        }
      }
    }
    m_nodes.render(ts,c);
  }
}

//----------------------------------------------------------

/** @brief Private data of the template engine */
class TemplateEngine::Private
{
    class IncludeEntry
    {
      public:
        enum Type { Template, Block };
        IncludeEntry(Type type,const QCString &fileName,const QCString &blockName,int line)
          : m_type(type), m_fileName(fileName), m_blockName(blockName), m_line(line) {}
        Type type() const { return m_type; }
        QCString fileName() const { return m_fileName; }
        QCString blockName() const { return m_blockName; }
        int line() const { return m_line; }

      private:
        Type m_type;
        QCString m_fileName;
        QCString m_blockName;
        int m_line;
    };
  public:
    Private(TemplateEngine *engine) : m_templateCache(17) /*, m_indent(0)*/, m_engine(engine)
    {
      m_templateCache.setAutoDelete(TRUE);
      m_includeStack.setAutoDelete(TRUE);
    }
    Template *loadByName(const QCString &fileName,int line)
    {
      //for (int i=0;i<m_indent;i++) printf("  ");
      //m_indent++;
      //printf("loadByName(%s,%d) {\n",fileName.data(),line);
      m_includeStack.append(new IncludeEntry(IncludeEntry::Template,fileName,QCString(),line));
      Template *templ = m_templateCache.find(fileName);
      if (templ==0)
      {
        QFile f(fileName);
        if (f.open(IO_ReadOnly))
        {
          uint size=f.size();
          char *data = new char[size+1];
          if (data)
          {
            data[size]=0;
            if (f.readBlock(data,f.size()))
            {
              templ = new TemplateImpl(m_engine,fileName,data);
              m_templateCache.insert(fileName,templ);
            }
            delete[] data;
          }
        }
        else
        {
          err("Cound not open template file %s\n",fileName.data());
        }
      }
      return templ;
    }
    void unload(Template * /*t*/)
    {
      //(void)t;
      //m_indent--;
      //for (int i=0;i<m_indent;i++) printf("  ");
      //printf("}\n");
      m_includeStack.removeLast();
    }

    void enterBlock(const QCString &fileName,const QCString &blockName,int line)
    {
      //for (int i=0;i<m_indent;i++) printf("  ");
      //m_indent++;
      //printf("enterBlock(%s,%s,%d) {\n",fileName.data(),blockName.data(),line);
      m_includeStack.append(new IncludeEntry(IncludeEntry::Block,fileName,blockName,line));
    }

    void leaveBlock()
    {
      //m_indent--;
      //for (int i=0;i<m_indent;i++) printf("  ");
      //printf("}\n");
      m_includeStack.removeLast();
    }

    void printIncludeContext(const char *fileName,int line) const
    {
      QListIterator<IncludeEntry> li(m_includeStack);
      li.toLast();
      IncludeEntry *ie=li.current();
      while ((ie=li.current()))
      {
        --li;
        IncludeEntry *next=li.current();
        if (ie->type()==IncludeEntry::Template)
        {
          if (next)
          {
            warn(fileName,line,"  inside template '%s' included from template '%s' at line %d",ie->fileName().data(),next->fileName().data(),ie->line());
          }
        }
        else // ie->type()==IncludeEntry::Block
        {
          warn(fileName,line,"  included by block '%s' inside template '%s' at line %d",ie->blockName().data(),
              ie->fileName().data(),ie->line());
        }
      }
    }

  private:
    QDict<Template> m_templateCache;
    //mutable int m_indent;
    TemplateEngine *m_engine;
    QList<IncludeEntry> m_includeStack;
};

TemplateEngine::TemplateEngine()
{
  p = new Private(this);
}

TemplateEngine::~TemplateEngine()
{
  delete p;
}

TemplateContext *TemplateEngine::createContext() const
{
  return new TemplateContextImpl(this);
}

Template *TemplateEngine::loadByName(const QCString &fileName,int line)
{
  return p->loadByName(fileName,line);
}

void TemplateEngine::unload(Template *t)
{
  p->unload(t);
}

void TemplateEngine::enterBlock(const QCString &fileName,const QCString &blockName,int line)
{
  p->enterBlock(fileName,blockName,line);
}

void TemplateEngine::leaveBlock()
{
  p->leaveBlock();
}

void TemplateEngine::printIncludeContext(const char *fileName,int line) const
{
  p->printIncludeContext(fileName,line);
}

