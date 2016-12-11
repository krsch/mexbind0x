#pragma once
#include <mex.h>
#include <typeinfo>
#include <type_traits>
#include <cstdint>
#include <string>
#include <complex>
#include <stdexcept>
#include <memory>
#include <vector>
#include <cassert>
#include "ndarray.h"
#ifdef __GNUC__
#include <cxxabi.h>
#endif
#include "func_types.h"

template<typename T> struct get_mex_classid;
using matlab_string = std::basic_string<mxChar>;

struct mx_array_t {
    mxArray * m;
    operator mxArray*() {
        return m;
    }
    operator const mxArray*() const {
        return m;
    }
    mx_array_t(const mxArray* m) : m(const_cast<mxArray*>(m)) {}
    mx_array_t(mxArray* m) : m(m) {}
};

template<typename T>
std::string get_type_name() {
    const char *name = typeid(T).name();
#ifdef __GNUC__
    int status;
    std::unique_ptr<char, decltype(std::free) *>
        demangled(abi::__cxa_demangle(name, 0, 0, &status), std::free);
    name = &*demangled;
#endif
    return {name};
}

template<> struct get_mex_classid<int8_t> : std::integral_constant<mxClassID, mxINT8_CLASS> {};
template<> struct get_mex_classid<uint8_t> : std::integral_constant<mxClassID, mxUINT8_CLASS> {};
template<> struct get_mex_classid<int16_t> : std::integral_constant<mxClassID, mxINT16_CLASS> {};
template<> struct get_mex_classid<uint16_t> : std::integral_constant<mxClassID, mxUINT16_CLASS> {};
template<> struct get_mex_classid<int32_t> : std::integral_constant<mxClassID, mxINT32_CLASS> {};
template<> struct get_mex_classid<uint32_t> : std::integral_constant<mxClassID, mxUINT32_CLASS> {};
template<> struct get_mex_classid<int64_t> : std::integral_constant<mxClassID, mxINT64_CLASS> {};
template<> struct get_mex_classid<uint64_t> : std::integral_constant<mxClassID, mxUINT64_CLASS> {};
template<> struct get_mex_classid<float> : std::integral_constant<mxClassID, mxSINGLE_CLASS> {};
template<> struct get_mex_classid<double> : std::integral_constant<mxClassID, mxDOUBLE_CLASS> {};
template<> struct get_mex_classid<mxChar> : std::integral_constant<mxClassID, mxCHAR_CLASS> {};
template<> struct get_mex_classid<mxLogical> : std::integral_constant<mxClassID, mxLOGICAL_CLASS> {};

template<typename F>
typename F::result_type mex_visit(F f, const mxArray* m) {
    switch (mxGetClassID(m)) {
        case mxINT8_CLASS:      return f.template run<int8_t>(m);
        case mxUINT8_CLASS:     return f.template run<uint8_t>(m);
        case mxINT16_CLASS:     return f.template run<int16_t>(m);
        case mxUINT16_CLASS:    return f.template run<uint16_t>(m);
        case mxINT32_CLASS:     return f.template run<int32_t>(m);
        case mxUINT32_CLASS:    return f.template run<uint32_t>(m);
        case mxINT64_CLASS:     return f.template run<int64_t>(m);
        case mxUINT64_CLASS:    return f.template run<uint64_t>(m);
        case mxSINGLE_CLASS:    return f.template run<float>(m);
        case mxDOUBLE_CLASS:    return f.template run<double>(m);
        case mxCHAR_CLASS:      return f.template run<mxChar>(m);
        case mxLOGICAL_CLASS:   return f.template run<mxLogical>(m);
        default:
            std::string s("Cannot convert ");
            s = s + mxGetClassName(m);
            throw std::invalid_argument(s);
    };
}

template<typename F>
struct mex_visit2_call {
    F f;
    using result_type = decltype(f((double*)0,(double*)0,(const mxArray*)0));
    template<typename T>
        result_type run(const mxArray *m) {
            return f((T*)mxGetData(m), (T*)mxGetImagData(m), m);
        }
};
template<typename F>
auto mex_visit2(F f, const mxArray *m) {
    return mex_visit(mex_visit2_call<F>{f}, m);
}

template<typename T, typename Type = void>
using enable_if_prim = typename std::enable_if<get_mex_classid<T>::value != mxUNKNOWN_CLASS, Type>::type;

template<typename T, typename Enable = void>
struct mex_cast_visitor;

// mex_cast primitive types
template<typename T>
struct mex_cast_visitor<T, std::enable_if_t<std::is_arithmetic<T>::value> > {
    typedef T result_type;
    template<typename V>
        T run(const mxArray *m) {
            if (!mxIsScalar(m)) throw std::invalid_argument("should be scalar");
            if (mxIsComplex(m)) throw std::invalid_argument("should be real");
            return static_cast<T>(*reinterpret_cast<const V*>(mxGetData(m)));
        }
};

// mex_cast flat collections
template<typename T>
struct mex_cast_visitor<T,enable_if_prim<typename T::value_type> > {
    typedef typename std::decay<T>::type result_type;
    template<typename V>
        T run(const mxArray *m) {
            if (mxIsComplex(m)) throw std::invalid_argument("should be real");
            const V* begin = reinterpret_cast<const V*>(mxGetData(m));
            const V* end = begin + mxGetNumberOfElements(m);
            return result_type(begin,end);
        }
};

template<typename T> T cast_ptr(const mxArray* m, void *ptr, int offset = 0) {
    mxClassID id = mxGetClassID(m);
    switch (id) {
        case mxINT8_CLASS: return (T)*(offset + (int8_t*)ptr);
        case mxUINT8_CLASS: return (T)*(offset + (uint8_t*)ptr);
        case mxINT16_CLASS: return (T)*(offset + (int16_t*)ptr);
        case mxUINT16_CLASS: return (T)*(offset + (uint16_t*)ptr);
        case mxINT32_CLASS: return (T)*(offset + (int32_t*)ptr);
        case mxUINT32_CLASS: return (T)*(offset + (uint32_t*)ptr);
        case mxINT64_CLASS: return (T)*(offset + (int64_t*)ptr);
        case mxUINT64_CLASS: return (T)*(offset + (uint64_t*)ptr);
        case mxSINGLE_CLASS: return (T)*(offset + (float*)ptr);
        case mxDOUBLE_CLASS: return (T)*(offset + (double*)ptr);
        case mxCHAR_CLASS: return (T)*(offset + (mxChar*)ptr);
        case mxLOGICAL_CLASS: return (T)*(offset + (mxLogical*)ptr);
        default:
            std::string s("Cannot convert ");
            const char *name = typeid(T).name();
            s = s + mxGetClassName(m) + " to " + name;
            throw std::invalid_argument(s);
    };
}

template<typename T> struct is_complex : public std::false_type {};

template<typename T> struct is_complex<std::complex<T>> : public std::true_type {
    typedef T type;
};

// mex_cast to mx_array_t
mx_array_t mex_cast(const mxArray *arg) {
    return {arg};
}

// mex_cast complex scalar
template<typename T>
typename std::enable_if<is_complex<T>::value, T >::type
mex_cast(const mxArray *arg)
{
    if (!mxIsScalar(arg))
        throw std::invalid_argument("should be scalar");
    typedef typename is_complex<T>::type U;
    return T(cast_ptr<T>(arg, mxGetData(arg)), cast_ptr<T>(arg, mxGetImagData(arg)));
}

// mex_cast generic pointer
template<typename T>
typename std::enable_if<std::is_pointer<T>::value, T>::type
mex_cast(const mxArray *arg)
{
    if (!mxIsInt8(arg) || mxGetNumberOfElements(arg) != sizeof(T))
        throw std::invalid_argument("Pointer should have been passed");
    return *(T*)mxGetData(arg);
}

// mex_cast matlab_string
template<typename T, typename = typename std::enable_if<std::is_same<T,matlab_string>::value>::type>
matlab_string mex_cast(const mxArray *arg)
{
    if (!mxIsChar(arg))
        throw std::invalid_argument("Expecting string");
    const mxChar *m = mxGetChars(arg);
    const int len = mxGetNumberOfElements(arg);
    return matlab_string(m, len);
}

// mex_cast classes that have can_mex_cast
// these are the classes constructible from const mxArray*
template<typename T>
typename std::enable_if<T::can_mex_cast, T>::type
mex_cast(const mxArray* a) {
    return T(a);
}

// mex_cast defined via mex_visitor
template<typename T>
typename mex_cast_visitor<T>::result_type
mex_cast(const mxArray *arg)
{
    try{
        return mex_visit(mex_cast_visitor<T>(),arg);
    } catch (...) {
        std::throw_with_nested(std::invalid_argument(
                "while converting to " + get_type_name<T>()));
    }
}

template<typename T, typename U>
std::enable_if_t<(vector_rank<T>::value > 1),T>
make_ndvector(NDArrayView<U,vector_rank<T>::value> v) {
    T res(v.max(0));
    for (int i=0; i<res.size(); i++)
        res[i] = make_ndvector<typename T::value_type>(v[i]);
    return res;
}

template<typename T, typename U>
std::enable_if_t<(vector_rank<T>::value == 1),T>
make_ndvector(NDArrayView<U,vector_rank<T>::value> v) {
    return T(v.begin(), v.end());
}

template<typename T>
std::enable_if_t<(vector_rank<T>::value > 1),T>
mex_cast(const mxArray *m)
{
    using value_type = ndvector_value_type_t<T>;
    NDArrayView<value_type,vector_rank<T>::value> v(m);
    return make_ndvector<T>(v);
}

template<typename T>
mxArray *to_mx_array(T* arg) {
    mxArray *res = mxCreateNumericMatrix(sizeof(arg), 1, mxINT8_CLASS, mxREAL);
    *(T**)mxGetData(res) = arg;
    return res;
}

template<typename T>
enable_if_prim<T,mxArray *> to_mx_array(T arg) {
    mxArray *res = mxCreateNumericMatrix(1, 1, get_mex_classid<T>::value, mxREAL);
    *(T*)mxGetData(res) = arg;
    return res;
}

template<typename T>
enable_if_prim<typename T::value_type,mxArray *> to_mx_array(const T& arg) {
    typedef typename T::value_type V;
    mxArray *res = mxCreateNumericMatrix(arg.size(), 1, get_mex_classid<V>::value, mxREAL);
    V* ptr = (V*)mxGetData(res);
    for (auto r : arg) *ptr++ = r;
    return res;
}

template<typename T>
void assign_ndvector(const std::vector<T> &vec, std::vector<mwIndex> iterator, int idx, mxArray *m) {
    for (int i=0; i<vec.size(); i++) {
        iterator[idx] = i;
        assign_ndvector(vec[i], iterator, idx+1, m);
    }
}

template<typename T, typename = enable_if_prim<T> >
void assign_ndvector(T val, std::vector<mwIndex> iterator, int idx, mxArray *m) {
    assert(idx == iterator.size());
    T *data = (T*)mxGetData(m);
    mwIndex i = mxCalcSingleSubscript(m, idx, iterator.data());
    data[i] = val;
}

template<typename T, typename = enable_if_prim<T> >
void assign_ndvector(std::complex<T> val, std::vector<mwIndex> iterator, int idx, mxArray *m) {
    assert(idx == iterator.size());
    T *data = (T*)mxGetData(m);
    T *imag_data = (T*)mxGetImagData(m);
    mwIndex i = mxCalcSingleSubscript(m, idx, iterator.data());
    data[i] = std::real(val);
    imag_data[i] = std::imag(val);
}

template<typename T>
mxArray *to_mx_array(const std::vector<T>& arg)
{
    auto sz_size_t = ndvector_size(arg);
    std::vector<mwIndex> sz(sz_size_t.begin(), sz_size_t.end());
    using value_type = typename ndvector_value_type<T>::type;
    mxComplexity c = is_complex<value_type>::value?mxCOMPLEX:mxREAL;
    mxArray *res = mxCreateNumericArray(sz.size(), sz.data(), get_mex_classid<value_type>::value, c);
    std::vector<mwIndex> iterator(sz.size());
    assign_ndvector(arg, iterator, 0, res);
    return res;
}

mxArray *to_mx_array(mx_array_t m) {
    return const_cast<mxArray *>(m.m); // MATLAB makes it impossible pass argument from input to output
}

template<typename V, typename T>
mxArray *convert_to_mx_array(const T& arg) {
    mxArray *res = mxCreateNumericMatrix(arg.size(), 1, get_mex_classid<V>::value, mxREAL);
    V* ptr = (V*)mxGetData(res);
    for (auto r : arg) *ptr++ = r;
    return res;
}

template<typename T> struct mx_store_by_move : std::false_type {};
#define STORED(x) template<> struct mx_store_by_move<x> : std::true_type {}

template<typename T>
typename std::enable_if<mx_store_by_move<T>::value,mxArray *>::type
to_mx_array(T&& arg)
{
    mxArray *res = mxCreateNumericMatrix(sizeof(T), 1, mxINT8_CLASS, mxREAL);
    new (mxGetData(res)) T(std::move(arg));
    return res;
}

template<typename T>
typename std::enable_if<mx_store_by_move<T>::value, const T&>::type
mex_cast(const mxArray *arg)
{
    if (!mxIsInt8(arg) || mxGetNumberOfElements(arg) != sizeof(T))
        throw std::invalid_argument("Pointer should have been passed");
    return *(T*)mxGetData(arg);
}

template<typename T>
std::enable_if_t<!is_complex<T>::value,T> cast_ptr_complex(const mxArray * m, int idx) {
    return cast_ptr<T>(m, mxGetData(m), idx);
}

template<typename T>
std::enable_if_t<is_complex<T>::value> cast_ptr_complex(const mxArray * m, int idx) {
    return T(
            cast_ptr<T>(m, mxGetData(m), idx),
            cast_ptr<T>(m, mxGetImagData(m), idx)
            );
}

class CellSaver;
template<typename T, typename = decltype(save_load(std::declval<CellSaver&>(), std::declval<T&>()))>
mxArray* to_mx_array(const T& t);

class CellSaver {
    std::vector<mx_array_t> cells;
public:
    operator mxArray*() const {
        mxArray *res = mxCreateCellMatrix(cells.size(), 1);
        for (int i=0; i<cells.size(); i++)
            mxSetCell(res, i, const_cast<mxArray*>(cells[i].m));
        return res;
    }
    operator mx_array_t() const {
        return {(mxArray *)*this};
    }
    template<typename T>
    CellSaver& operator<<(const T& t) {
        cells.push_back(to_mx_array(t));
        return *this;
    }
    template<typename T>
        CellSaver& operator&(const T& t) {
            return *this << std::forward<const T&>(t);
        }
};

class CellLoader {
    const mxArray *m;
    int idx = 0;
public:
    CellLoader(const mxArray *m) : m(m) {}
    template<typename T>
        CellLoader& operator>>(T& t) {
            t = mex_cast<T>(mxGetCell(m, idx++));
            return *this;
        }
    template<typename T>
        CellLoader& operator&(T& t) {
            return *this >> std::forward<T&>(t);
        }
};

template<typename T, typename>
mxArray* to_mx_array(const T& t) {
    CellSaver c;
    save_load(c,const_cast<T&>(t));
    return c;
}

template<typename T, typename = decltype(save_load(std::declval<CellLoader&>(), std::declval<T&>()))>
std::decay_t<T> mex_cast(const mxArray *m) {
    CellLoader c(m);
    T t;
    save_load(c,t);
    return t;
}
