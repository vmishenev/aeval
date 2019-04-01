#include <exception>
#include "ae/AeValSolver.hpp"
#include "ae/MarshallVisitor.hpp"
#include "ufo/Smt/EZ3.hh"
#include "SynthLib2ParserIFace.hpp"


using namespace SynthLib2Parser;
using namespace ufo;

/** An AE-VAL wrapper for cmd
 *
 * Usage: specify 2 smt2-files that describe the formula \foral x. S(x) => \exists y . T (x, y)
 *   <s_part.smt2> = S-part (over x)
 *   <t_part.smt2> = T-part (over x, y)
 *   --skol = to print skolem function
 *   --debug = to print more info and perform sanity checks
 *
 * Notably, the tool automatically recognizes x and y based on their appearances in S or T.
 *
 * Example:
 *
 * ./tools/aeval/aeval
 *   ../test/ae/example1_s_part.smt2
 *   ../test/ae/example1_t_part.smt2
 *
 */


bool getBoolValue(const char *opt, bool defValue, int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], opt) == 0) return true;
    }
    return defValue;
}

char *getSmtFileName(int num, int argc, char **argv)
{
    int num1 = 1;
    for (int i = 1; i < argc; i++)
    {
        int len = strlen(argv[i]);
        if (len >= 5 && strcmp(argv[i] + len - 5, ".smt2") == 0)
        {
            if (num1 == num) return argv[i];
            else num1++;
        }
    }
    return NULL;
}

char *getSlFileName(int num, int argc, char **argv)
{
    int num1 = 1;
    for (int i = 1; i < argc; i++)
    {
        int len = strlen(argv[i]);
        if (len >= 5 && strcmp(argv[i] + len - 3, ".sl") == 0)
        {
            if (num1 == num) return argv[i];
            else num1++;
        }
    }
    return NULL;
}

int main (int argc, char **argv)
{

    ExprFactory efac;


    bool skol = getBoolValue("--skol", false, argc, argv);
    bool allincl = getBoolValue("--all-inclusive", false, argc, argv);
    bool compact = getBoolValue("--compact", false, argc, argv);
    bool debug = getBoolValue("--debug", false, argc, argv);
    bool sl = getBoolValue("--sl", false, argc, argv);
    if(sl) // for synth-lib format
    {
        char *fname = getSlFileName(1, argc, argv);
        cout << "read file " << fname << endl;
        try
        {
            ae::MarshallVisitor::Solve(fname, efac, skol, allincl);
        }
        catch (const std::exception &Ex)
        {
            std::cerr << "Error " << endl;
            std::cerr << Ex.what() << endl;
            exit(1);
        }
        return 0;

    }

    EZ3 z3(efac);
    Expr s = z3_from_smtlib_file (z3, getSmtFileName(1, argc, argv));
    std::cout << "----------" << std::endl;
    Expr t = z3_from_smtlib_file (z3, getSmtFileName(2, argc, argv));

    if (allincl)
        getAllInclusiveSkolem(s, t, debug, compact);
    else
        aeSolveAndSkolemize(s, t, skol, debug, compact);

    return 0;
}

