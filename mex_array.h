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
        return cast_ptr_complex<T>(m, idx);
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

template<typename T>
struct MXTyped1DArray {
    const mxArray *m;
    MXTyped1DArray(const mxArray *m) : m(m) {}

    T operator[](int idx) {
        return cast_ptr_complex<T>(m,idx);
    }
};