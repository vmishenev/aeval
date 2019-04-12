#if !defined __MARSHALL_VISITOR_HPP
#define __MARSHALL_VISITOR_HPP

#include <list>
#include <map>
#include <exception>
#include <iostream>
#include <set>

#include "../../synthlib2parser/src/include/SynthLib2ParserIFace.hpp"
#include "../ufo/Smt/Z3n.hpp"
#include "../ufo/ExprLlvm.hpp"
#include "AeValSolver.hpp"
#include "AeValException.hpp"

using namespace ufo;

namespace ae
{

    enum Theories { LIA, Reals };

    // Utility function for name mangling
    inline std::string MangleName(const std::string &Name, int param)
    {
        //Retval += "_@_" + ActualSort->ToMangleString();
        return Name + "_@_" + std::to_string(param);
    }

    bool iequals(const string &a, const string &b)
    {
        unsigned int sz = a.size();
        if (b.size() != sz)
            return false;
        for (unsigned int i = 0; i < sz; ++i)
            if (tolower(a[i]) != tolower(b[i]))
                return false;
        return true;
    }
    struct FunDef
    {
        //TODo constructor for check size names and sorts
        std::string Name;
        std::vector< Expr  > ArgNames;
        std::vector< Expr > ArgSort;
        Expr ReturnSort;
        Expr Body;
    };

    class MarshallVisitor : public SynthLib2Parser::ASTVisitorBase
    {
    private:
        Theories CurrentTheory;
        ExprFactory &efac;
        typedef Expr SortType ;
        typedef Expr FnDecl ;
        /*[+] Stacks*/
        std::vector< SortType > SortStack;
        std::vector<Expr> ProcessedTermStack;
        /*[-] Stacks*/
        bool InFunDef;
        bool InSynthFun;
        bool InSortDef;
        bool InConstraintCmd;

        std::map<std::string, Expr> VarExpr;
        std::map<std::string, FnDecl> SynthFunExpr;

        std::map<std::string, Expr> SynthFunName_Expr; // replace synth-fun with variable

        std::vector<Expr> Constaraints;
        std::set<Expr> SynthFnsVars;
        std::multimap<std::string, FunDef> DefFns;

        std::map<std::string, Expr> ArgMap;

        std::vector<std::map<std::string, Expr>> LetVarExpressionStack;
        /*vector<map<Expression, Expression>> LetVarBindingStack;*/

    public:
        MarshallVisitor(ExprFactory &efac);
        virtual ~MarshallVisitor();

        // Visit methods
        virtual void VisitProgram(const SynthLib2Parser::Program *Prog) override;


        virtual void VisitSetLogicCmd(const SynthLib2Parser::SetLogicCmd *Cmd) override;
        virtual void VisitConstraintCmd(const SynthLib2Parser::ConstraintCmd *Cmd) override;
        virtual void VisitSynthFunCmd(const SynthLib2Parser::SynthFunCmd *Cmd) override;
        virtual void VisitVarDeclCmd(const SynthLib2Parser::VarDeclCmd *Cmd) override;
        virtual void VisitFunDeclCmd(const SynthLib2Parser::FunDeclCmd *Cmd) override;

        virtual void VisitCheckSynthCmd(const SynthLib2Parser::CheckSynthCmd *Cmd) override;

        virtual void VisitFunTerm(const SynthLib2Parser::FunTerm *TheTerm) override;
        virtual void VisitLiteralTerm(const SynthLib2Parser::LiteralTerm *TheTerm) override;
        virtual void VisitSymbolTerm(const SynthLib2Parser::SymbolTerm *TheTerm) override;
        virtual void VisitFunDefCmd(const SynthLib2Parser::FunDefCmd *Cmd) override;
        virtual void VisitLetTerm(const SynthLib2Parser::LetTerm *TheTerm) override;



        /*
          virtual void VisitSortDefCmd(const SynthLib2Parser::SortDefCmd *Cmd) override;
          virtual void VisitSetOptsCmd(const SynthLib2Parser::SetOptsCmd *Cmd) override;
          */

        /*[+] Syntatic constrains (grammar)*/

        /* virtual void VisitFunGTerm(const SynthLib2Parser::FunGTerm *TheTerm) override;
         virtual void VisitLiteralGTerm(const SynthLib2Parser::LiteralGTerm *TheTerm) override;
         virtual void VisitSymbolGTerm(const SynthLib2Parser::SymbolGTerm *TheTerm) override;
         virtual void VisitLetGTerm(const SynthLib2Parser::LetGTerm *TheTerm) override;
         virtual void VisitConstantGTerm(const SynthLib2Parser::ConstantGTerm *TheTerm) override;
         virtual void VisitVariableGTerm(const SynthLib2Parser::VariableGTerm *TheTerm) override;*/

        /*[-] Syntatic constrains (grammar)*/

        /*[+] Sort EXPR*/

        virtual void VisitIntSortExpr(const SynthLib2Parser::IntSortExpr *Sort) override;
        virtual void VisitBVSortExpr(const SynthLib2Parser::BVSortExpr *Sort) override;
        virtual void VisitNamedSortExpr(const SynthLib2Parser::NamedSortExpr *Sort) override;
        virtual void VisitArraySortExpr(const SynthLib2Parser::ArraySortExpr *Sort) override;
        virtual void VisitRealSortExpr(const SynthLib2Parser::RealSortExpr *Sort) override;
        virtual void VisitFunSortExpr(const SynthLib2Parser::FunSortExpr *Sort) override;
        virtual void VisitBoolSortExpr(const SynthLib2Parser::BoolSortExpr *Sort) override;
        virtual void VisitEnumSortExpr(const SynthLib2Parser::EnumSortExpr *Sort) override;

        /*[-] Sort EXPR*/

        static void Solve(const string &InFileName, ExprFactory &efac, bool allincl, bool scol );

    };

    using namespace SynthLib2Parser;

    void MarshallVisitor::Solve(const string &InFileName, ExprFactory &efac, bool allincl, bool scol )
    {
        MarshallVisitor AeSynth(efac);
        SynthLib2Parser::SynthLib2Parser Parser;
        std::cout << "try read file" << std::endl;
        try
        {
            Parser(InFileName);
        }
        catch (const std::exception &Ex)
        {
            cout << "Error " << endl;
            cout << Ex.what() << endl;
            throw Ex;
        }
        std::cout << "readed file" << std::endl;
        Parser.GetProgram()->Accept(&AeSynth);
        return;
    }

    MarshallVisitor::MarshallVisitor(ExprFactory &efac) :
        ASTVisitorBase("AeValSolver"),
        efac(efac),
        InFunDef(false),
        InSynthFun(false),
        InSortDef(false),
        InConstraintCmd(false)
    {
        // Nothing here
    }

    MarshallVisitor::~MarshallVisitor()
    {
        // Nothing here
    }

    void MarshallVisitor::VisitProgram(const Program *Prog)
    {
        ASTVisitorBase::VisitProgram(Prog);
    }

    void MarshallVisitor::VisitLetTerm(const LetTerm *TheTerm)
    {
        //throw UnimplementedException((string)"Let-term are not supported yet.");
        std::cout << "visit let term" << std::endl;
        std::map<std::string, Expr> NewLetVarExpressionMap;

        auto const &Bindings = TheTerm->GetBindings();
        const size_t NumBindings = Bindings.size();
        for (size_t i = 0; i < NumBindings; ++i)
        {
            // TODO ckeck sort
            Bindings[i]->GetVarSort()->Accept(this);
            //auto CurSort = SortStack.back(); //unused
            SortStack.pop_back();
            auto const &CurVarName = Bindings[i]->GetVarName();

            Bindings[i]->Accept(this);

            NewLetVarExpressionMap[CurVarName] = ProcessedTermStack.back();
            ProcessedTermStack.pop_back();
        }

        LetVarExpressionStack.push_back(NewLetVarExpressionMap);

        TheTerm->GetBoundInTerm()->Accept(this);
        // Pop the stacks
        LetVarExpressionStack.pop_back();


    }

    void MarshallVisitor::VisitFunDefCmd(const FunDefCmd *Cmd)
    {
        InFunDef = true;
        // Populate the arg maps
        ArgMap.clear();
        //uint32 CurPosition = 0;
        std::vector<SortType> ArgTypes;
        std::vector<Expr> ArgNames;
        for (auto const ASPair : Cmd->GetArgs())
        {
            if (ArgMap.find(ASPair->GetName()) != ArgMap.end())
            {
                throw AeValException("Error, parameter identifer \"" + ASPair->GetName() +
                                     "\" has been reused");
            }
            // Visit the sort
            ASPair->GetSort()->Accept(this);
            auto Type = SortStack.back();
            ArgTypes.push_back(Type);
            ArgNames.push_back(mkTerm<std::string>(ASPair->GetName(), efac) );
            SortStack.pop_back();
            ArgMap[ASPair->GetName()] =  mkTerm<std::string>(ASPair->GetName(), efac);
        }
        // Process the return type
        Cmd->GetSort()->Accept(this);
        auto Type = SortStack.back();
        SortStack.pop_back();
        // now process the term
        Cmd->GetTerm()->Accept(this);
        // Create the function
        auto Exp = ProcessedTermStack.back();
        ProcessedTermStack.pop_back();


        DefFns.emplace(std::pair<std::string, FunDef>( Cmd->GetFunName(), FunDef{Cmd->GetFunName(), ArgNames, ArgTypes, Type, Exp}));

        std::cout << "def-fun " << Cmd->GetFunName() << "(" << ArgNames.size() << ") " << Exp << std::endl;
        InFunDef = false;
        ArgMap.clear();
    }

    void MarshallVisitor::VisitFunDeclCmd(const FunDeclCmd *Cmd)
    {
        throw UnimplementedException((string)"Uninterpreted functions are not supported yet.");
    }

    void MarshallVisitor::VisitSetLogicCmd(const SetLogicCmd *Cmd)
    {
        ASTVisitorBase::VisitSetLogicCmd(Cmd);
        if(Cmd->GetLogicName() == "LIA")
        {
            CurrentTheory = Theories::LIA;
        }
        else  if(Cmd->GetLogicName() == "Reals")
        {
            CurrentTheory = Theories::Reals;
        }
        else
        {
            throw UnimplementedException("Unsupported theory");
        }
    }


    void MarshallVisitor::VisitVarDeclCmd(const VarDeclCmd *Cmd)
    {
        ASTVisitorBase::VisitVarDeclCmd(Cmd);
        // Register the variable
        if (SortStack.size() != 1)
        {
            throw AeValException((string)"Internal Error: Expected to see exactly one sort on stack!");
        }
        std::string name = Cmd->GetName();
        ExprVector type;
        type.push_back(SortStack[0]);

        Expr VarNameExpr = mkTerm<std::string>(name, efac);
        Expr expr = bind::fdecl (VarNameExpr, type);
        SortStack.pop_back();
        VarExpr.insert(std::pair<std::string, Expr>(name, expr));
        std::cout << "decl var " << name << " = " << expr << std::endl;

    }

    void MarshallVisitor::VisitConstraintCmd(const ConstraintCmd *Cmd)
    {
        InConstraintCmd = true;
        ASTVisitorBase::VisitConstraintCmd(Cmd); //gather terms to ProcessedTermStack
        InConstraintCmd = false;

        if (ProcessedTermStack.size() != 1)
        {
            throw AeValException("Expected one term on stack!");
        }

        Expr ConstraintExpression = ProcessedTermStack.back();
        ProcessedTermStack.pop_back();
        //TODO check Bool Sort of Expression

        std::cout << "ConstraintExpression " << ConstraintExpression << std::endl;
        Constaraints.push_back(ConstraintExpression);
    }

    void MarshallVisitor::VisitLiteralTerm(const LiteralTerm *TheTerm)
    {
        auto TheLit = TheTerm->GetLiteral();
        std::string snum = TheLit->GetLiteralString();
        std::cout << "# VisitLiteralTerm: " << TheLit->GetLiteralString() << std::endl;
        Expr e;
        if(iequals(snum, "true"))
        {
            e = mk<TRUE> (efac);
        }
        else if(iequals(snum, "false"))
        {
            e = mk<FALSE> (efac);
        }
        else if(CurrentTheory == Theories::Reals)
            e =  mkTerm (mpq_class (snum), efac);
        else if(CurrentTheory == Theories::LIA)
            e =  mkTerm (mpz_class (snum), efac);
        else
            throw AeValException("Literal of Unsupported theory");
        ProcessedTermStack.push_back(e);
        //TheLit->GetSort()->Accept(this);
        //  auto Type = SortStack.back();
        //  SortStack.pop_back();


        /*auto Val = Solver->CreateValue(Type, TheLit->GetLiteralString());
        auto Exp = Solver->CreateExpression(Val);
        ProcessedTermStack.push_back(Exp);*/
    }

    void MarshallVisitor::VisitFunTerm(const FunTerm *TheTerm)
    {
        ASTVisitorBase::VisitFunTerm(TheTerm);


        // The arguments to this term must be on top of the stack now
        const size_t NumArgs = TheTerm->GetArgs().size();
        ExprVector args(NumArgs);
        std::cout << "VisitFunTerm: " << TheTerm->GetFunName() << " args " << NumArgs << std::endl;

        if(ProcessedTermStack.size() < NumArgs)
        {
            throw new AeValException("Incorrect arbitry of function application");

        }

        for (size_t i = 0; i < NumArgs; ++i)
        {
            args[NumArgs - i - 1] = ProcessedTermStack.back();
            ProcessedTermStack.pop_back();
        }
        Expr e;
        bool isFound = false;
        std::string FnName =   TheTerm->GetFunName();
        // Find fun term in Synth functions
        //auto it = SynthFunExpr.find (FnName);
        std::string MangledFnName = MangleName(FnName, NumArgs);
        auto it = SynthFunName_Expr.find (MangledFnName);
        if (it != SynthFunName_Expr.end ())
        {
            ExprVector EmptyArgs;
            e =  bind::fapp (it->second, EmptyArgs);
            SynthFnsVars.emplace(e);
            isFound = true;
        }

        if( !isFound)   // Find funTerm in Defined functions
        {

            auto result_defs = DefFns.equal_range(FnName);
            //TODO ckeck sorts of call and definition sorts
            //for( auto fd_pair :result_defs) {

            for (auto it = result_defs.first; it != result_defs.second; it++)
            {
                FunDef df = it->second;
                std::cout << "!!! find " << df.Body << std::endl;
                if(df.ArgNames.size() !=  NumArgs)
                    continue;
                // substitution
                ExprVector ArgNameTerms(NumArgs);
                e = replaceAll(df.Body, df.ArgNames, args );

                isFound = true;

            }

        }
        if(!isFound)      // Find funTerm in Theory functions
        {
            isFound = true;
            if(FnName ==  "not" && NumArgs == 1)
            {
                e = mk<NEG>  (*args.begin());
            }
            else if(FnName ==  "and")
            {
                e = mknary<AND> (args.begin(), args.end());
            }
            else if(FnName ==  "or")
            {
                e = mknary<OR> (args.begin(), args.end());
            }
            else if(FnName ==  "=>")
            {
                e = mknary<IMPL> (args.begin(), args.end());
            }
            else if(FnName ==  "=")
            {
                e = mknary<EQ> (args.begin(), args.end());
            }
            else if(FnName ==  "<")
            {
                e =  mknary<LT> (args.begin(), args.end());
            }
            else if(FnName ==  "<=")
            {
                e =  mknary<LEQ> (args.begin(), args.end());
            }
            else if(FnName ==  ">")
            {
                e =  mknary<GT> (args.begin(), args.end());
            }
            else if(FnName ==  ">=")
            {
                e =  mknary<GEQ> (args.begin(), args.end());
            }
            else if(FnName ==  "+")
            {
                e =  mknary<PLUS> (args.begin(), args.end());
            }
            else if(FnName ==  "-")
            {
                e =  mknary<MINUS> (args.begin(), args.end());
            }
            else if(FnName ==  "*")
            {
                e =  mknary<MULT> (args.begin(), args.end());
            }
            else if(FnName ==  "/")
            {
                e =  mknary<DIV> (args.begin(), args.end());
            }
            else if(FnName ==  "%")
            {
                e =  mknary<MOD> (args.begin(), args.end());
            }
            else if(FnName ==  "ite")
            {
                e =  mknary<ITE> (args.begin(), args.end());
            }
            else
            {
                isFound = false;

            }

        }

        if(isFound)
        {
            ProcessedTermStack.push_back(e);
            std::cout << "ProcessedTermStack : " << e << std::endl;
            return;
        }
        else
        {
            throw AeValException(std::string("Not found FunTerm: ") + FnName);
        }

    }

    void MarshallVisitor::VisitSynthFunCmd(const SynthFunCmd *Cmd)
    {

        InSynthFun = true;
        std::string FnName = Cmd->GetFunName();
        // Ignore Grammar

        // The return type
        Cmd->GetSort()->Accept(this);
        auto RetType = SortStack.back();
        SortStack.pop_back();

        // The formal params now
        vector<string> ParamNames;
        //std::vector<SortType > ArgTypes;
        ExprVector ArgTypes;

        for (auto const &ASPair : Cmd->GetArgs())
        {
            ASPair->GetSort()->Accept(this);
            auto Type = SortStack.back();
            ArgTypes.push_back(Type);
            ParamNames.push_back(ASPair->GetName());
            SortStack.pop_back();
        }
        ArgTypes.push_back(RetType);

        Expr FnNameExpr = mkTerm<std::string>(FnName, efac);
        FnDecl decl = bind::fdecl (FnNameExpr, ArgTypes);
        SynthFunExpr.insert(std::pair<std::string, FnDecl>(FnName, decl));

        std::cout << "synth fun decl " << FnName << " = " << decl << std::endl;

        /*[+] replace synth-fun with variable*/
        std::string MangledFnName = MangleName(FnName, Cmd->GetArgs().size());
        Expr MangledFnNameExpr = mkTerm<std::string>(MangledFnName, efac);
        ExprVector NewArgTypes;
        NewArgTypes.push_back(RetType);
        Expr VarDecl = bind::fdecl (MangledFnNameExpr, NewArgTypes);
        SynthFunName_Expr.insert(std::pair<std::string, Expr>(MangledFnName, VarDecl));
        std::cout << "## replaced with  " << FnName << " = " << VarDecl << std::endl;
        /*[-] replace synth-fun with variable*/


        InSynthFun = false;
    }

    void MarshallVisitor::VisitSymbolTerm(const SymbolTerm *TheTerm)
    {
        std::cout << "VisitSymbolTerm: " << TheTerm->GetSymbol() << std::endl;

        std::string VarName = TheTerm->GetSymbol();

        /*Find as Let-Binding*/
        for (auto it = LetVarExpressionStack.rbegin(); it != LetVarExpressionStack.rend(); ++it)
        {
            auto const &CurMap = *it;
            auto MapIt = CurMap.find(VarName);
            if (MapIt != CurMap.end())
            {
                ProcessedTermStack.push_back(MapIt->second);
                return;
            }
        }

        /*Find as Arg*/
        if (InFunDef)
        {
            auto it = ArgMap.find(VarName);
            if (it != ArgMap.end())
            {
                ExprVector args;
                Expr e =  bind::fapp (it->second, args);
                ProcessedTermStack.push_back(e);
                return;
            }
        }



        /*Find Var*/
        auto it = VarExpr.find (VarName);
        if (it != VarExpr.end ())
        {
            ExprVector args;
            Expr e =  bind::fapp (it->second, args);
            ProcessedTermStack.push_back(e);
        }
        else
        {
            throw AeValException("Not found var");
        }
    }

    void MarshallVisitor::VisitCheckSynthCmd(const CheckSynthCmd *Cmd)
    {
        std::cout << "VisitCheckSynthCmd:" << Constaraints.size() << std::endl;
        Expr e ;

        if(Constaraints.size() > 1)
            e = mknary<AND> (Constaraints.begin(), Constaraints.end());
        else
            e = *(Constaraints.begin());

        std::cout << "Constaraints: " << e << std::endl;

        //std::cout<< "sizes:   "<<SynthFnsVars.size()<<std::endl;
        //Expr e3 = *(SynthFnsVars.begin());
        // std::cout<<e3<<std::endl;
        /*ExprVector ExistVars;
        /* for(auto &a:SynthFnsVars) {
               ExistVars.emplace_back(a);
        }*/
        //std::copy(SynthFnsVars.begin(), SynthFnsVarsw.end(), ExistVars.begin());

        /* ExprVector AllVars;
         filter (e, bind::IsConst (), back_inserter (AllVars));
         // divide ALL vars into existentially quantified vars and all-ly var
         std::cout<< "---Alll vars----- "<<std::endl;

         //for (auto &a: AllVars) std::cout << *a  << isOpX<FAPP> (a)  <<( a== e3 )<< ", ";
        // std::cout<<std::endl;
         std::cout<< "--- End Alll vars----- "<<std::endl;*/

        ufo::aeSolveAndSkolemize(e, SynthFnsVars, true, true, false);
    }




    /* ************
        [+] Sort EXPR
    ****************/

    void MarshallVisitor::VisitIntSortExpr(const IntSortExpr *Sort)
    {

        auto Type = sort::intTy (efac);
        if (InSortDef)
        {
            //BindType(SortName, Type);
        }
        else
        {
            SortStack.push_back(Type);
        }
    }
    void MarshallVisitor::VisitRealSortExpr(const RealSortExpr *Sort)
    {
        auto Type = sort::realTy (efac);
        if (InSortDef)
        {
            //BindType(SortName, Type);
        }
        else
        {
            SortStack.push_back(Type);
        }
    }

    void MarshallVisitor::VisitBoolSortExpr(const BoolSortExpr *Sort)
    {
        auto Type = sort::boolTy (efac);
        if (InSortDef)
        {
            //BindType(SortName, Type);
        }
        else
        {
            SortStack.push_back(Type);
        }
    }




    void MarshallVisitor::VisitBVSortExpr(const BVSortExpr *Sort)
    {
        throw UnimplementedException((string)"BVS sorts are not yet supported");
    }



    void MarshallVisitor::VisitNamedSortExpr(const NamedSortExpr *Sort)
    {
        throw UnimplementedException((string)"Named sorts are not yet supported");
    }

    void MarshallVisitor::VisitArraySortExpr(const ArraySortExpr *Sort)
    {
        throw UnimplementedException((string)"Arrays are not yet supported");
    }



    void MarshallVisitor::VisitFunSortExpr(const FunSortExpr *Sort)
    {
        throw UnimplementedException((string)"Function sorts are not yet supported");
    }

    void MarshallVisitor::VisitEnumSortExpr(const EnumSortExpr *Sort)
    {
        throw UnimplementedException((string)"Enum sorts are not yet supported");
    }
    /*[-] Sort EXPR*/


} /* End namespace */

#endif