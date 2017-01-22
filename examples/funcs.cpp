#include <mex_commands.h>
#include <vector>

using namespace mexbind0x;

int add(int a, int b)
{
    return a+b;
}

void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray * prhs[])
{
    try {
        MXCommands m(nlhs,plhs,nrhs,prhs);
        m.on("add", add);
        m.on("sub", [](int a, int b) { return a-b; });
        m.on_varargout("divmod", [](int nargout, int a, int b)
                       -> std::vector<mx_auto> {
            //std::vector<mx_auto> res;
            if (nargout == 2) return {a/b, a%b};
            else if (nargout < 2) return {a/b};
            else throw std::invalid_argument("too many output arguments");
        });
        if (!m.has_matched())
            throw std::invalid_argument("Command not found");
    } catch (...) {
        flatten_exception();
    }
}
