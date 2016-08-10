#pragma once
#include <mex.h>
#include <stdexcept>
#include <complex>
#include <vector>
#include <typeinfo>
#include <memory>
#include <cstring>

class Reader {
    public:
        virtual void parse(const mxArray*) = 0;
        virtual const char* name() const = 0;
        virtual const char* description() const = 0;
};

template<typename T, T val> struct static_val {
    static constexpr T v = val;
};

template<typename T> struct get_mex_classid;

template<> struct get_mex_classid<int8_t> : static_val<mxClassID, mxINT8_CLASS> {};
template<> struct get_mex_classid<uint8_t> : static_val<mxClassID, mxUINT8_CLASS> {};
template<> struct get_mex_classid<int16_t> : static_val<mxClassID, mxINT16_CLASS> {};
template<> struct get_mex_classid<uint16_t> : static_val<mxClassID, mxUINT16_CLASS> {};
template<> struct get_mex_classid<int32_t> : static_val<mxClassID, mxINT32_CLASS> {};
template<> struct get_mex_classid<uint32_t> : static_val<mxClassID, mxUINT32_CLASS> {};
template<> struct get_mex_classid<float> : static_val<mxClassID, mxSINGLE_CLASS> {};
template<> struct get_mex_classid<double> : static_val<mxClassID, mxDOUBLE_CLASS> {};
template<> struct get_mex_classid<mxChar> : static_val<mxClassID, mxCHAR_CLASS> {};
template<> struct get_mex_classid<mxLogical> : static_val<mxClassID, mxLOGICAL_CLASS> {};

//template<typename T>
//struct get_mex_classid<std::complex<T>> : get_mex_classid<T> {};

template<typename T>
bool mex_is_class(mxArray *arg) {
    return get_mex_classid<T>::v == mxGetClassID(arg);
}

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

struct MXArray {
    const mxArray *m;
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

//template<typename T> T mex_cast(const mxArray *arg);

template<typename T, typename = typename std::enable_if<std::is_same<T,MXArray>::value>::type>
MXArray mex_cast(const mxArray *m) {
    return MXArray{m};
}

template<typename T> struct is_complex;

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
typename std::enable_if<get_mex_classid<T>::v >=0, T>::type
mex_cast(const mxArray *arg)
{
    if (!mxIsScalar(arg))
        throw std::invalid_argument("should be scalar");
    return cast_ptr<T>(arg, mxGetData(arg));
}

template<typename T>
class ReaderImpl : public virtual Reader {
    T& val;
    std::string var_name;
    const char *descr;
    public:
        ReaderImpl(T& val, const char *name, const char *description)
            : val(val)
            , var_name(name)
            , descr(description)
        {}
        virtual void parse(const mxArray* arg) override {
            try {
                val = mex_cast<T>(arg);
            } catch (const std::invalid_argument &e) {
                std::string s("Error in parsing ");
                s = s + var_name + ": " + e.what();
                throw std::invalid_argument(s);
            }
        }
        virtual const char *name() const override {
            return var_name.c_str();
        }
        virtual const char *description() const override {
            return descr;
        }
};

struct MexParams {
    std::vector<std::unique_ptr<Reader>> readers;
    const char *name;
    MexParams(const char *name) : readers(), name(name) {}
    void parse(int nrhs, const mxArray* prhs[]) {
        try {
            if (nrhs != readers.size())
                throw std::invalid_argument("bad number of arguments");
            for (int i=0; i<nrhs; i++)
                readers[i]->parse(prhs[i]);
        } catch (std::exception &e) {
            mexWarnMsgTxt(usage().c_str());
            throw;
        }
    }

    template<typename T>
        MexParams& add(T& v, const char *name, const char *description)
        {
            readers.emplace_back(new ReaderImpl<T>(v, name, description));
        }

    std::string usage() const {
        std::string s;
        s = s + "Usage: " + name + "(";
        for (const auto &r : readers)
            s = s + r->name() + ", ";
        if (readers.size() > 0) s.erase(s.end()-2, s.end());
        s = s + ")\n";
        for (const auto &r : readers)
            if (r->description() != 0)
                s = s + r->name() + ": " + r->description() + "\n";
        return s;
    }
};


// http://stackoverflow.com/questions/23999573/convert-a-number-to-a-string-literal-with-constexpr
namespace detail
{
    template<unsigned... digits>
    struct to_chars { static const char value[]; };

    template<unsigned... digits>
    const char to_chars<digits...>::value[] = {('0' + digits)..., 0};

    template<unsigned rem, unsigned... digits>
    struct explode : explode<rem / 10, rem % 10, digits...> {};

    template<unsigned... digits>
    struct explode<0, digits...> : to_chars<digits...> {};
}

template<unsigned num>
struct num_to_string : detail::explode<num> {};

template<typename T, T* f, int i>
struct mex_name {
    static constexpr const char *name = num_to_string<i+1>::value;
};

template<typename T, T* f>
struct mex_all_names {
    static constexpr const char *v = 0;
};

template<typename T, T* f, int i>
struct mex_description {
    static constexpr const char *v = 0;
};

template<typename T> struct args_of;
template<typename R, typename ... Args>
struct args_of<R (Args...)> {
    typedef std::tuple<Args...> type;
};

template<typename T> struct return_of;
template<typename R, typename ... Args>
struct return_of<R (Args...)> {
    typedef R type;
};

template<int i, int n, typename T, T* f>
typename std::enable_if< i<n >::type
fillParamsIdx(MexParams& params, typename args_of<T>::type& tup)
{
    std::string name = mex_name<T,f,i>::name;
    name = "arg" + name;
    if (mex_all_names<T,f>::v != 0) {
        const char * s = mex_all_names<T,f>::v;
        for (int j=i; j>0 && s!=0; j--) s = strchr(s+1, ',');
        if (s!=0) {
            const char *end = strchr(s+1,',');
            if (end == 0) name.assign(s+1, s+strlen(s)-1);
            else name.assign(s+1,end);
        }
    }
    params.add(std::get<i>(tup), name.c_str(), mex_description<T,f,i>::v);
    fillParamsIdx<i+1,n,T,f>(params, tup);
}

template<int i, int n, typename T, T* f>
typename std::enable_if< i==n >::type
fillParamsIdx(MexParams& params, typename args_of<T>::type& tup) {}

#define NAMES(f,names) template<> struct mex_all_names<decltype(f),f> { static constexpr const char *v = names; }
#define EXPORT(ret,f,args) ret f args; NAMES(f,#args); ret f args
#define DESCRIBE(f,i,val) template<> struct mex_description<decltype(f), f,i> { static constexpr const char *v = val; }

//http://stackoverflow.com/questions/7858817/unpacking-a-tuple-to-call-a-matching-function-pointer
template<int ...>
struct seq { };

template<int N, int ...S>
struct gens : gens<N-1, N-1, S...> { };

template<int ...S>
struct gens<0, S...> {
  typedef seq<S...> type;
};

template<typename F, typename Tup, int ...S>
typename return_of<F>::type callFunc(F *func, const Tup& params, seq<S...>) {
    return func(std::get<S>(params) ...);
}

template<typename T>
struct to_tuple {
    typedef std::tuple<T> type;
};
template<typename ... Args>
struct to_tuple<std::tuple<Args...>> {
    typedef std::tuple<Args...> type;
};

template<typename T>
mxArray *to_mx_array(T* arg) {
    mxArray *res = mxCreateNumericMatrix(sizeof(arg), 1, mxINT8_CLASS, mxREAL);
    *(T**)mxGetData(res) = arg;
    return res;
}

template<int i=0, typename Tup>
typename std::enable_if< i == std::tuple_size<Tup>::value, void>::type
save_tuple(const Tup &tup, int nlhs, mxArray *plhs[])
{}

template<int i=0, typename Tup>
typename std::enable_if< i < std::tuple_size<Tup>::value, void>::type
save_tuple(const Tup &tup, int nlhs, mxArray *plhs[])
{
    if (i < nlhs || i==0 && nlhs==0) {
        plhs[i] = to_mx_array(std::get<i>(tup));
        save_tuple<i+1,Tup>(tup, nlhs, plhs);
    }
}

template<typename F, F* f>
void mexIt(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
    typedef typename args_of<F>::type Args;
    typedef typename return_of<F>::type Result;
    typedef typename gens<std::tuple_size<Args>::value>::type Seq;
    Args args;
    MexParams params("");
    fillParamsIdx<0, std::tuple_size<Args>::value, F, f>(params, args);
    params.parse(nrhs, prhs);
    Result res = callFunc(f,args,Seq());
    typename to_tuple<Result>::type res_t(res);
    save_tuple(res_t, nlhs, plhs);
}

