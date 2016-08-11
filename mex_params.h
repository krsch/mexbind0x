#pragma once
#include "mex_cast.h"
#include <mex.h>
#include <stdexcept>
#include <complex>
#include <vector>
#include <memory>
#include <cstring>
#include <map>

class Reader {
    public:
        virtual void parse(const mxArray*) = 0;
        virtual const char* name() const = 0;
        virtual const char* description() const = 0;
};

//template<typename T>
//struct get_mex_classid<std::complex<T>> : get_mex_classid<T> {};

template<typename T>
bool mex_is_class(mxArray *arg) {
    return get_mex_classid<T>::value == mxGetClassID(arg);
}

//template<typename T, mxClassID value = get_mex_classid<T>::value>
//constexpr bool mxIsPrim() {
    //return value != mxUNKNOWN_CLASS;
//}
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

template<typename T, typename = typename std::enable_if<std::is_same<T,MXArray>::value>::type>
MXArray mex_cast(const mxArray *m) {
    return MXArray{m};
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

template<typename T, typename Enable = void> struct mx_decay_t : std::decay<T> {};
template<typename T>
struct mx_decay_t<T, typename std::enable_if<mx_store_by_move<T>::value>::type>
{
    typedef T type;
};
template<typename T>
using mx_decay = typename mx_decay_t<T>::type;

template<typename T> struct args_of;
template<typename R, typename ... Args>
struct args_of<R (Args...)> {
    typedef std::tuple<mx_decay<Args>...> type;
};

template<typename T> struct return_of_t;
template<typename R, typename ... Args>
struct return_of_t<R (Args...)> {
    typedef R type;
};

template<typename T>
using return_of = typename return_of_t<T>::type;

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
return_of<F> callFunc(F *func, const Tup& params, seq<S...>) {
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

template<int i=0, typename Tup>
typename std::enable_if< i == std::tuple_size<Tup>::value, void>::type
save_tuple(Tup &&tup, int nlhs, mxArray *plhs[])
{}

template<int i=0, typename Tup>
typename std::enable_if< i < std::tuple_size<Tup>::value, void>::type
save_tuple(Tup &&tup, int nlhs, mxArray *plhs[])
{
    if (i < nlhs || i==0 && nlhs==0) {
        plhs[i] = to_mx_array(std::get<i>(tup));
        save_tuple<i+1,Tup&&>(std::move(tup), nlhs, plhs);
    }
}

template<typename F, F* f>
return_of<F> runIt(int nrhs, const mxArray *prhs[]) {
    typedef typename args_of<F>::type Args;
    typedef typename gens<std::tuple_size<Args>::value>::type Seq;
    Args args;
    MexParams params("");
    fillParamsIdx<0, std::tuple_size<Args>::value, F, f>(params, args);
    params.parse(nrhs, prhs);
    return callFunc(f,args,Seq());
}

template<typename F, F* f>
typename std::enable_if<std::is_same<return_of<F>,void>::value>::type
mexIt(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
    runIt<F,f>(nrhs, prhs);
}

template<typename F, F* f>
typename std::enable_if<0 < std::tuple_size<return_of<F> >::value,void>::type
mexIt(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
    typedef return_of<F> Result;
    Result res = runIt<F,f>(nrhs, prhs);
    save_tuple(std::move(res), nlhs, plhs);
}

template<typename F, F* f>
typename std::enable_if<!std::is_same<return_of<F>,void>::value>::type
mexIt(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
    auto res = runIt<F,f>(nrhs, prhs);
    plhs[0] = to_mx_array(std::move(res));
}

struct MexCommands {
    typedef void (*command)(int,mxArray**,int,const mxArray**);
    std::map<std::string, std::vector<std::pair<int,command>>> commands;

    template<typename T, T* f>
        void add(const char *name) {
            std::string s(name);
            int nlhs = std::tuple_size<typename to_tuple<return_of<T>>::type>::value;
            commands[s].push_back({nlhs, mexIt<T,f>});
        }

    void dispatch(int nlhs, mxArray** plhs, int nrhs, const mxArray** prhs) {
        if (nrhs < 1 || !mxIsChar(prhs[0]))
            throw std::invalid_argument("First argument should be a command");
        char *command_s = mxArrayToString(prhs[0]);
        std::string com(command_s);
        mxFree(command_s);
        auto it = commands.find(com);
        if (it == commands.end())
            throw std::invalid_argument("Unknown command");
        if (nlhs == 0) nlhs = 1;
        int best_n = 0;
        command best;
        for (auto &c : it->second)
            if (c.first <= nlhs && c.first >= best_n) {
                best_n = c.first;
                best = c.second;
            }
        best(nlhs, plhs, nrhs-1, prhs+1);
    }
};

#define ADD_COMMAND(c,f,name) c.add<decltype(f),f>(name)
