#pragma once
#include "mex_params.h"
#include <stdexcept>
#include <string>
#include <vector>

class command_required : std::exception {
    public:
        using exception::exception;
};

template<typename F, typename ... Args>
auto wrap_varargout(F&& f, int nargout, types_t<int,Args...>)
{
    return [f,nargout](Args ... args) {
        return f(nargout,args...);
    };
}

// MXCommands allows you to dispatch a function based on argin[0]
class MXCommands {
    int nargout;
    mxArray **argout;
    int nargin;
    const mxArray **argin;
    std::string command;
    public:
        MXCommands(int nargout, mxArray *argout[], int nargin, const mxArray *argin[])
            : nargout(nargout), argout(argout), nargin(nargin-1), argin(argin+1)
        {
            if (nargin < 1 || !mxIsChar(argin[0]))
                mexErrMsgIdAndTxt("code:command_required", "First argument should be a command");
            char *command_s = mxArrayToString(argin[0]);
            command = command_s;
            mxFree(command_s);
        }

        template<typename F>
        MXCommands& on(const char *command_, F&& f) {
            if (command == command_)
                try {
                    mexIt(f,nargout, argout, nargin, argin);
                } catch (const std::exception &e) {
                    std::throw_with_nested(
                            std::invalid_argument(
                                stringer("When calling \"",command,'"')
                                )
                            );
                }
            return *this;
        }

        template<typename F>
        MXCommands& on_varargout(const char *command_, F&& f) {
            if (command == command_) {
                try {
                    std::vector<mxArray *> res = runIt(wrap_varargout(f,nargout,args_of(f)),nargin,argin);
                    if (nargout != res.size() && (nargout != 0 || res.size() != 1))
                        throw std::invalid_argument("cannot assign all output arguments");
                    for (int i=0;i <res.size(); i++)
                        argout[i] = res[i];
                } catch (const std::exception &e) {
                    std::throw_with_nested(
                            std::invalid_argument(
                                stringer("When calling \"",command,'"')
                                )
                            );
                }
            }
            return *this;
        }

        const std::string& get_command() {
            return command;
        }
};
