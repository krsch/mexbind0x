#pragma once
#include "mex_cast.h"
#include <mex.h>
#include <stdexcept>
#include <complex>
#include <vector>
#include <memory>
#include <cstring>
#include <map>

class argument_cast_exception : std::exception {
    public:
        int arg_idx = -1;
        using exception::exception;
};

template<typename T>
bool mex_is_class(mxArray *arg) {
    return get_mex_classid<T>::value == mxGetClassID(arg);
}

struct MXArray {
    const mxArray *m;
    MXArray(const mxArray *m) : m(m) {}
    template<typename T, typename ... Args>
    T get (Args ... args) {
        const mwSize *dim = mxGetDimensions(m);
        int n = mxGetNumberOfDimensions(m);
        int idx = get_idx(n,dim,args...);
        return cast_ptr<T>(m, mxGetData(m), idx);
    }

    template<typename T, typename ... Args>
    std::complex<T> getc (Args ... args) {
        const mwSize *dim = mxGetDimensions(m);
        int n = mxGetNumberOfDimensions(m);
        int idx = get_idx(n,dim,args...);
        return std::complex<T>(
                cast_ptr<T>(m, mxGetData(m), idx),
                cast_ptr<T>(m, mxGetImagData(m), idx)
                );
    }

    template<typename T, typename ... Args>
    T geti (Args ... args) {
        const mwSize *dim = mxGetDimensions(m);
        int n = mxGetNumberOfDimensions(m);
        int idx = get_idx(n,dim,args...);
        return cast_ptr<T>(m, mxGetImagData(m), idx);
    }

    int get_idx(int n, const mwSize*) {
        if (n != 0)
            throw std::out_of_range("bad number of dimensions");
        return 0;
    }

    template<typename T, typename ... Args>
        int get_idx(int n, const mwSize* dim, T t, Args ... args) {
            return *dim * t + get_idx(n-1, dim+1, args...);
        }
};

template<typename T> struct return_of_t;
template<typename R, typename ... Args>
struct return_of_t<R (Args...)> {
    typedef R type;
};
template<typename R, typename ... Args>
struct return_of_t<R (*)(Args...)> {
    typedef R type;
};

template<typename T>
using return_of = typename return_of_t<T>::type;

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
struct type_t {
    typedef T type;
};

template<typename ... Args>
struct types_t {
    typedef std::tuple<Args...> tuple_t;
    static constexpr int size = sizeof...(Args);
};

template<int i, typename T>
types_t<std::pair<T,std::integral_constant<int,i>>> count_args(types_t<T>) {
    return {};
}

template<typename T, typename ...Args>
types_t<T,Args...>
tuple_cons(type_t<T>, types_t<Args...>) { return {}; } 

template<int i=0, typename T, typename T2, typename ... Args>
auto count_args(types_t<T,T2,Args...>) {
    auto tail = count_args<i+1>(types_t<T2,Args...>());
    type_t<std::pair<T, std::integral_constant<int,i>>> head;
    return tuple_cons(head,tail);
}

template<typename R, typename ... Args>
types_t<std::decay_t<Args>...> args_of(R f(Args...)) {
    return {};
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
