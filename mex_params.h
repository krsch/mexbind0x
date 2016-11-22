#pragma once
#include "mex_cast.h"
#include "mex_array.h"
#include "func_types.h"
#include <mex.h>
#include <stdexcept>

class argument_cast_exception : std::exception {
    public:
        int arg_idx = -1;
        using exception::exception;
};

template<typename T>
bool mex_is_class(mxArray *arg) {
    return get_mex_classid<T>::value == mxGetClassID(arg);
}

template<int i=0, typename Tup>
typename std::enable_if< i == std::tuple_size<Tup>::value, void>::type
save_tuple(Tup &&tup, int nlhs, mxArray *plhs[])
{}

template<int i=0, typename Tup>
typename std::enable_if< i < std::tuple_size<Tup>::value, void>::type
save_tuple(Tup &&tup, int nlhs, mxArray *plhs[])
{
    if (i < nlhs || (i==0 && nlhs==0)) {
        plhs[i] = to_mx_array(std::get<i>(tup));
        save_tuple<i+1,Tup&&>(std::move(tup), nlhs, plhs);
    }
}

template<typename T>
typename T::first_type get_array(const mxArray* a[]) {
    try {
        return mex_cast<typename T::first_type>(a[T::second_type::value]);
    } catch (argument_cast_exception &e) {
        e.arg_idx = T::second_type::value;
        throw;
    }
}

template<typename F, typename ... Args>
auto callFuncArgs(F f, const mxArray* prhs[], types_t<Args...>) {
    return f(get_array<Args>(prhs)...);
}

template<typename F>
auto runIt(F f, int nrhs, const mxArray *prhs[]) {
    auto counted_args = count_args(args_of(f));
    if (counted_args.size != nrhs)
        throw std::invalid_argument("number of arguments mismatch");
    return callFuncArgs(f, prhs, counted_args);
}

template<typename F>
typename std::enable_if<std::is_same<return_of<F>,void>::value>::type
mexIt(F f, int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
    runIt(f,nrhs, prhs);
}

template<typename F>
typename std::enable_if<0 < std::tuple_size<return_of<F> >::value,void>::type
mexIt(F f, int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
    typedef return_of<F> Result;
    Result res = runIt(f, nrhs, prhs);
    save_tuple(std::move(res), nlhs, plhs);
}

template<typename F>
typename std::enable_if<!std::is_same<return_of<F>,void>::value>::type
mexIt(F f, int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
    auto res = runIt(f,nrhs, prhs);
    plhs[0] = to_mx_array(std::move(res));
}
