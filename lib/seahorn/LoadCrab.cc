#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "seahorn/config.h"

namespace seahorn
{
  using namespace llvm;

  /// Loads Crab invariants into  a Horn Solver
  class LoadCrab: public llvm::ModulePass
  {
  public:
    static char ID;
    
    LoadCrab () : ModulePass(ID) {}
    virtual ~LoadCrab () {}
    
    virtual bool runOnModule (Module &M);
    virtual bool runOnFunction (Function &F);
    virtual void getAnalysisUsage (AnalysisUsage &AU) const;
    virtual const char* getPassName () const {return "LoadCrab";}
  };
  
  char LoadCrab::ID = 0;
  Pass* createLoadCrabPass () {return new LoadCrab ();}

} // end namespace seahorn

#ifndef HAVE_CRAB_LLVM
// dummy implementation when Crab is not compiled in
namespace seahorn
{
  void LoadCrab::getAnalysisUsage (AnalysisUsage &AU) const
  {AU.setPreservesAll ();}

  bool LoadCrab::runOnModule (Module &M)
  {
    errs () << "WARNING: Not loading invariants. Compiled without Crab support.\n";
    return false;
  }
  bool LoadCrab::runOnFunction (Function &F) {return false;}
}
#else
  // real implementation starts here 
#include "ufo/Expr.hpp"
#include "ufo/ExprLlvm.hpp"

#include <crab_llvm/CfgBuilder.hh>
#include <crab_llvm/CrabLlvm.hh>

#include "seahorn/HornifyModule.hh"

#include "llvm/Support/CommandLine.h"

#include "boost/lexical_cast.hpp"

namespace llvm
{
   template <typename Number, typename VariableName>
   llvm::raw_ostream& operator<< (llvm::raw_ostream& o, 
                                  crab::cfg_impl::z_lin_cst_t cst)
   {
     std::ostringstream s;
     s << cst;
     o << s.str ();
     return o;
   }

   inline llvm::raw_ostream& operator<< (llvm::raw_ostream& o, 
                                         crab::cfg_impl::z_lin_cst_sys_t csts)
   {
     std::ostringstream s;
     s << csts;
     o << s.str ();
     return o;
   }

} // end namespace

namespace seahorn
{
  namespace crab_smt 
  {
    // TODO: marshal expr::Expr to crab::linear_constraints

    using namespace llvm;
    using namespace expr;
    using namespace crab::cfg_impl;

    struct FailUnMarshal
    {
      static Expr unmarshal (const z_lin_cst_t &cst, ExprFactory &efac)
      { 
        llvm::errs () << "Cannot unmarshal: " << cst << "\n";
        assert (0); exit (1); 
      }
    };
      
    template <typename U>
    struct BasicExprUnMarshal
    {
      // Crab does not distinguish between bools and the rest of
      // integers but SeaHorn does.

      // A normalizer for Boolean constraints
      class BoolCst 
      {
        typedef enum {T_TRUE, T_FALSE, T_TOP} tribool_t;
      
        tribool_t      m_val;

        // internal representation:
        // constraint is of the form m_coef*m_var {<=,==} m_rhs
        // m_coef and m_rhs can be negative numbers.

        ikos::z_number m_coef;
        const Value*   m_var;
        bool           m_is_eq; //tt:equality, ff:inequality
        ikos::z_number m_rhs;
        
       public:
        
        // If cst is a constraint of the form x<=c where x is a LLVM
        // Value of type i1 then return x, otherwise null.
        static const Value* isBoolCst (z_lin_cst_t cst)
        {
          if (cst.is_disequation ()) return nullptr;
          auto e = cst.expression() - cst.expression().constant();
          if (std::distance (e.begin (), e.end ()) != 1) return nullptr; 
          auto t = *(e.begin ());
          varname_t v = t.second.name();
          assert (v.get () && "Cannot have shadow vars");
          if ( (*(v.get ()))->getType ()->isIntegerTy (1))
            return *(v.get ()); 
          else return nullptr; 
        }
        
        BoolCst (z_lin_cst_t cst): m_val (T_TOP),
                                         m_coef (0), m_rhs (0), 
                                         m_var (nullptr), m_is_eq (cst.is_equality ())
        {
          assert (isBoolCst (cst));

          auto e = cst.expression() - cst.expression().constant();
          auto t = *(e.begin ());
          assert (t.second.name ().get () && "Cannot have shadow vars");
          m_var  = *(t.second.name ().get ());
          m_coef = t.first;
          m_rhs  = -cst.expression().constant();
          
          if (m_is_eq)
          {
            if (m_rhs == 0)                        /* k*x == 0 for any k*/
            { m_val = T_FALSE; }
            else if (m_coef == 1 && m_rhs == 1)    /*x == 1*/
            { m_val = T_TRUE; }
            else if (m_coef == -1 && m_rhs == -1)  /*-x == -1*/
            { m_val = T_TRUE; }
          }
        }
        
        // Conjoin two boolean constraints
        void operator+= (BoolCst other)
        {
          // they cannot disagree otherwise the initial constraint
          // would be false.
          assert (!(m_val == T_TRUE  && other.m_val == T_FALSE));
          assert (!(m_val == T_FALSE && other.m_val == T_TRUE));
          
          if (m_val != T_TOP && other.m_val == T_TOP) 
            return; 
          if (m_val == T_TOP && other.m_val != T_TOP)
          { 
            m_val = other.m_val; 
            return;  
          }
          
          if (!m_is_eq && !other.m_is_eq) // both are inequalities
          {
            
            if ( ( (m_coef == 1 && m_rhs == 0) &&               /* x <= 0*/
                   (other.m_coef == -1 && other.m_rhs == 0)) || /*-x <= 0*/ 
                 ( (m_coef == -1 && m_rhs == 0) &&              /*-x <= 0*/
                   (other.m_coef == 1 && other.m_rhs == 0)))    /* x <= 0*/
            { m_val = T_FALSE; }
            else if ( ((m_coef == 1 && m_rhs == 1) &&                /*x <= 1*/
                       (other.m_coef == -1 && other.m_rhs == -1)) || /*-x <=-1*/ 
                      ((m_coef == -1 && m_rhs == -1) &&              /*-x <=-1*/
                       (other.m_coef == 1 && other.m_rhs == 1)))     /*x <= 1*/
            {  m_val = T_TRUE; } 
          }
        }
        
        bool isUnknown () const { return m_val == T_TOP; }
        
        Expr toExpr (ExprFactory &efac) const 
        {
          if (isUnknown ()) return mk<TRUE>(efac);

          Expr e = mkTerm<const Value*>(m_var, efac);
          e = bind::boolConst (e);
          if (m_val == T_FALSE) return mk<NEG> (e);
          else return e;
        }
        
      };
      

      typedef DenseMap<const Value*, BoolCst> bool_map_t;
      bool_map_t bool_map;
      
      Expr unmarshal_num( ikos::z_number n, ExprFactory &efac)
      {
        const mpz_class mpz ((mpz_class) n);
        return mkTerm (mpz, efac);
      }
       
      Expr unmarshal_int_var( varname_t v, ExprFactory &efac)
      {
        assert (v.get () && "Cannot have shadow vars");
        Expr e = mkTerm<const Value*>(*(v.get()), efac);
        return bind::intConst (e);
      }
       
      Expr unmarshal (z_lin_cst_t cst, ExprFactory &efac)
      {
        if (cst.is_tautology ())     
          return mk<TRUE> (efac);

        if (cst.is_contradiction ()) 
          return mk<FALSE> (efac);

        // booleans
        if (const Value* v = BoolCst::isBoolCst (cst))
        {
          BoolCst b2 (cst);
          auto it = bool_map.find (v);
          if (it != bool_map.end ())
          {
            BoolCst &b1 = it->second;
            b1 += b2;
          }
          else { bool_map.insert (make_pair (v, b2)); } 

          return mk<TRUE> (efac); // we ignore cst for now
        }
        
        // integers
        auto e = cst.expression() - cst.expression().constant();
        Expr ee = unmarshal_num ( ikos::z_number ("0"), efac);
        for (auto t : e)
        {
          ikos::z_number n  = t.first;
          varname_t v = t.second.name();
          if (n == 0) continue;
          else if (n == 1) 
            ee = mk<PLUS> (ee, unmarshal_int_var (v, efac));
          else if (n == -1) 
            ee = mk<MINUS> (ee, unmarshal_int_var (v, efac));
          else
            ee = mk<PLUS> (ee, mk<MULT> ( unmarshal_num (n, efac), 
                                          unmarshal_int_var (v, efac)));
        }

        ikos::z_number c = -cst.expression().constant();
        Expr cc = unmarshal_num (c, efac);
        if (cst.is_inequality ())
          return mk<LEQ> (ee, cc);
        else if (cst.is_equality ())
          return mk<EQ> (ee, cc);        
        else 
          return mk<NEQ> (ee, cc);
        
      }
       
      Expr unmarshal (z_lin_cst_sys_t csts, ExprFactory &efac)
      {
        Expr e = mk<TRUE> (efac);

        // integers
        for (auto cst: csts)
        { e = boolop::land (e, unmarshal (cst, efac)); } 

        // booleans 
        for (auto p: bool_map)
        {
          auto b = p.second;
          if (!b.isUnknown ()) { e = boolop::land (e, b.toExpr (efac)); }
        }

        return e;
      }
    };

  } // end namespace crab_smt

} // end namespace seahorn


namespace seahorn
{
  using namespace llvm;
  using namespace crab::cfg_impl;
  
  
  bool LoadCrab::runOnModule (Module &M)
  {
    for (auto &F : M) runOnFunction (F);
    return true;
  }
  
  expr::Expr Convert (crab_llvm::CrabLlvm &crab,
                      const llvm::BasicBlock *BB, 
                      const expr::ExprVector &live, 
                      expr::ExprFactory &efac) 
  {
    // FIXME: crab [BB] returns actually inv_tbl_t that for now it is
    // z_lin_cst_sys_t but this might change
    z_lin_cst_sys_t csts = crab [BB];
    crab_smt::BasicExprUnMarshal < crab_smt::FailUnMarshal > c;
    expr::Expr inv = c.unmarshal (csts, efac);

    if ( (std::distance (live.begin (), live.end ()) == 0) && 
         (!expr::isOpX<expr::FALSE> (inv)))
      return expr::mk<expr::TRUE> (efac); 
    else 
      return inv;
  }

  bool LoadCrab::runOnFunction (Function &F)
  {
    HornifyModule &hm = getAnalysis<HornifyModule> ();
    crab_llvm::CrabLlvm &crab = getAnalysis<crab_llvm::CrabLlvm> ();
    
    auto &db = hm.getHornClauseDB ();
    
    for (auto &BB : F)
    {
      // skip all basic blocks that HornifyModule does not know
      if (! hm.hasBbPredicate (BB)) continue;
      
      const ExprVector &live = hm.live (BB);
      
      Expr pred = hm.bbPredicate (BB);
      Expr inv = Convert (crab, &BB, live, hm.getExprFactory ());

      LOG ("crab", 
           errs () << "Loading invariant " << *bind::fname (pred);
           errs () << "("; for (auto v: live) errs () << *v << " ";
           errs () << ")  "  << *inv << "\n"; );
           

      db.addConstraint (bind::fapp (pred, live), inv);
      
    }
    return true;
  }
  
  
  void LoadCrab::getAnalysisUsage (AnalysisUsage &AU) const
  {
    AU.setPreservesAll ();
    AU.addRequired<HornifyModule> ();
    AU.addRequired<crab_llvm::CrabLlvm> ();
  }
  
}
#endif
