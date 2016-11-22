#pragma once
#include "mex_cast.h"
#include <complex>

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

    template<typename T>
        T get1(size_t idx) {
            return cast_ptr<T>(m, mxGetData(m), idx);
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


