#ifndef OVERLOAD_HPP
#define OVERLOAD_HPP

template<class...> struct overload;

template<> struct overload<> {
  void operator()() const;
};

template<class F, class ... Fs>
struct overload<F, Fs...>: F, overload<Fs...> {
  using F::operator();
  using overload<Fs...>::operator();

  overload(F f, Fs...fs): F{f}, overload<Fs...>{fs...} { }
};

#endif
