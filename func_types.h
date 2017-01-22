#pragma once
#include <tuple>

template<typename T, typename Enable = void> struct return_of_t;
template<typename R, typename ... Args>
struct return_of_t<R (Args...)> {
    typedef R type;
};
template<typename R, typename ... Args>
struct return_of_t<R (*)(Args...)> {
    typedef R type;
};
template<typename F, typename R, typename ... Args>
struct return_of_t<R (F::*)(Args...)> {
    typedef R type;
};
template<typename F, typename R, typename ... Args>
struct return_of_t<R (F::*)(Args...) const> {
    typedef R type;
};
template<typename F, typename = decltype(&F::operator())>
using is_functor = void;

template<typename F>
struct return_of_t<F,is_functor<F>> {
    typedef typename return_of_t<decltype(&F::operator())>::type type;
};

template<typename T>
using return_of = typename return_of_t<T>::type;

template<typename T>
struct type_t {
    typedef T type;
};

template<typename ... Args>
struct types_t {
    typedef std::tuple<Args...> tuple_t;
    static constexpr int size = sizeof...(Args);
};

template<int i=0, typename T>
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

template<typename F, typename R, typename ... Args>
types_t<std::decay_t<Args>...> args_of_member(R (F::*f)(Args...)) {
    return {};
}

template<typename F, typename R, typename ... Args>
types_t<std::decay_t<Args>...> args_of_member(R (F::*f)(Args...) const) {
    return {};
}

template<typename F, typename = decltype(&F::operator())>
auto args_of(F f) {
    return args_of_member(&F::operator());
}

template<typename F, typename R, typename ... Args>
types_t<F*,std::decay_t<Args>...> args_of(R (F::*f)(Args...)) {
    return {};
}

template<typename F, typename R, typename ... Args>
types_t<F*,std::decay_t<Args>...> args_of(R (F::*f)(Args...) const) {
    return {};
}

template<typename R, typename ... Args>
types_t<std::decay_t<Args>...> args_of(R f(Args...)) {
    return {};
}

template<typename T>
void deleter(T* arg) {
    delete arg;
}

template<typename T>
void deleter2(const T& arg)
{
    arg.~T();
}

#include <vector>
template<typename T>
struct vector_rank : public std::integral_constant<size_t, 0> {};
template<typename T>
struct vector_rank<std::vector<T>> 
    : public std::integral_constant<size_t, vector_rank<T>::value + 1> {};

void calc_ndvector_size(...) {}

template<typename T, typename It>
void calc_ndvector_size(It it, const std::vector<T> &vec) {
    *it = vec.size();
    //for (const T &v : vec)
        //if (v.size() != vec[0].size())
            //throw std::invalid_argument("ndvector_size does only accepts rectangular-like multidimensional vectors");
    calc_ndvector_size(it+1, vec[0]);
}

template<typename T>
std::vector<size_t> ndvector_size(const std::vector<T> &vec)
{
    std::vector<size_t> res(vector_rank<T>::value+1);
    calc_ndvector_size(res.begin(), vec);
    return res;
}

template<typename T>
struct ndvector_value_type : public type_t<T> {};
template<typename T>
struct ndvector_value_type<std::vector<T>> 
    : public ndvector_value_type<T> {};
template<typename T>
using ndvector_value_type_t = typename ndvector_value_type<T>::type;
