#pragma once
#include <mex.h>
#include <typeinfo>
#include <type_traits>
#include <cstdint>
#include <string>
#include <complex>
#include <stdexcept>

template<typename T> struct get_mex_classid;
using matlab_string = std::basic_string<mxChar>;

template<> struct get_mex_classid<int8_t> : std::integral_constant<mxClassID, mxINT8_CLASS> {};
template<> struct get_mex_classid<uint8_t> : std::integral_constant<mxClassID, mxUINT8_CLASS> {};
template<> struct get_mex_classid<int16_t> : std::integral_constant<mxClassID, mxINT16_CLASS> {};
template<> struct get_mex_classid<uint16_t> : std::integral_constant<mxClassID, mxUINT16_CLASS> {};
template<> struct get_mex_classid<int32_t> : std::integral_constant<mxClassID, mxINT32_CLASS> {};
template<> struct get_mex_classid<uint32_t> : std::integral_constant<mxClassID, mxUINT32_CLASS> {};
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

template<typename T, typename Type = void>
using enable_if_prim = typename std::enable_if<get_mex_classid<T>::value != mxUNKNOWN_CLASS, Type>::type;

template<typename T, typename Enable = void>
struct mex_cast_visitor;

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
        case mxSINGLE_CLASS: return (T)*(offset + (float*)ptr);
        case mxDOUBLE_CLASS: return (T)*(offset + (double*)ptr);
        case mxCHAR_CLASS: return (T)*(offset + (mxChar*)ptr);
        case mxLOGICAL_CLASS: return (T)*(offset + (mxLogical*)ptr);
        default:
            std::string s("Cannot convert ");
            s = s + mxGetClassName(m) + " to " + typeid(T).name();
            throw std::invalid_argument(s);
    };
}

template<typename T> struct is_complex {
    static constexpr bool v = false;
};

template<typename T> struct is_complex<std::complex<T>> {
    typedef T type;
    static constexpr bool v = true;
};

template<typename T>
typename std::enable_if<is_complex<T>::v, T >::type
mex_cast(const mxArray *arg)
{
    if (!mxIsScalar(arg))
        throw std::invalid_argument("should be scalar");
    typedef typename is_complex<T>::type U;
    return T(cast_ptr<T>(arg, mxGetData(arg)), cast_ptr<T>(arg, mxGetImagData(arg)));
}

template<typename T>
typename std::enable_if<std::is_pointer<T>::value, T>::type
mex_cast(const mxArray *arg)
{
    if (!mxIsInt8(arg) || mxGetNumberOfElements(arg) != sizeof(T))
        throw std::invalid_argument("Pointer should have been passed");
    return *(T*)mxGetData(arg);
}

template<typename T, typename = typename std::enable_if<std::is_same<T,matlab_string>::value>::type>
matlab_string mex_cast(const mxArray *arg)
{
    if (!mxIsChar(arg))
        throw std::invalid_argument("Expecting string");
    const mxChar *m = mxGetChars(arg);
    const int len = mxGetNumberOfElements(arg);
    return matlab_string(m, len);
}

template<typename T>
typename std::enable_if<T::can_mex_cast, T>::type
mex_cast(const mxArray* a) {
    return T(a);
}

template<typename T>
typename mex_cast_visitor<T>::result_type
mex_cast(const mxArray *arg)
{
    return mex_visit(mex_cast_visitor<T>(),arg);
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
}

template<typename T>
enable_if_prim<typename T::value_type,mxArray *> to_mx_array(const T& arg) {
    typedef typename T::value_type V;
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
std::enable_if_t<!is_complex<T>::v,T> cast_ptr_complex(const mxArray * m, int idx) {
    return cast_ptr<T>(m, mxGetData(m), idx);
}

template<typename T>
std::enable_if_t<is_complex<T>::v> cast_ptr_complex(const mxArray * m, int idx) {
    return T(
            cast_ptr<T>(m, mxGetData(m), idx),
            cast_ptr<T>(m, mxGetImagData(m), idx)
            );
}

