#pragma once
#include "mex_params.h"
#include <stdexcept>
#include <string>

class command_required : std::exception {
    public:
        using exception::exception;
};

// MXCommands allows you to dispatch a function based on prhs[0]
class MXCommands {
    int nlhs;
    mxArray **plhs;
    int nrhs;
    const mxArray **prhs;
    std::string command;
    public:
        MXCommands(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
            : nlhs(nlhs), plhs(plhs), nrhs(nrhs-1), prhs(prhs+1)
        {
            if (nrhs < 1 || !mxIsChar(prhs[0]))
                mexErrMsgIdAndTxt("code:command_required", "First argument should be a command");
            char *command_s = mxArrayToString(prhs[0]);
            command = command_s;
            mxFree(command_s);
        }

        template<typename F>
        MXCommands& on(const char *command_, F&& f) {
            if (command == command_)
                mexIt(f,nlhs, plhs, nrhs, prhs);
            return *this;
        }

        const std::string& get_command() {
            return command;
        }
};
