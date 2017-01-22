#include <mex_commands.h>
#include <vector>

using namespace mexbind0x;

int add(int a, int b)
{
    return a+b;
}

void mex(MXCommands &m) {
    m.on("add", add);
    m.on("sub", [](int a, int b) { return a-b; });
    m.on_varargout("divmod", [](int nargout, int a, int b)
                   -> std::vector<mx_auto> {
                       //std::vector<mx_auto> res;
                       if (nargout == 2) return {a/b, a%b};
                       else if (nargout < 2) return {a/b};
                       else throw std::invalid_argument("too many output arguments");
                   });
}

MEX_SIMPLE(mex);
