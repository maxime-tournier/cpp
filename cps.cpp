#include <functional>

#include <iostream>


namespace cps {

	template<class R, class A>
	using cont = std::function<R(A)>;
	
	template<class R, class A>
	using monad = std::function<R(cont<R, A>)>;

	template<class A>
	struct pure_type {
		const A a;

		template<class Cont>
		auto operator()(const Cont& cont) const {
			return cont(a);
		}
	};


	template<class A>
	static pure_type<A> pure(const A& a) { return {a}; }
	
	template<class CA, class Func>
	struct bind_type {
		const CA ca;
		const Func func;
		
		template<class Cont>
		auto operator()(const Cont& cont) const {
			return ca([&](const auto& a) {
				return func(a)(cont);
			});
		}
	};

	template<class CA, class Func>
	static bind_type<CA, Func> operator>>(const CA& ca, const Func& func) {
		return {ca, func};
	}

	template<class Func, class Cont>
	auto run(const Func& func, const Cont& cont) {
		return func(cont);
	}


	struct yield {
		template<class Cont>
		Cont operator()(const Cont& cont) const {
			return cont;
		}
	};
	
}



int main(int, char**) {

	using namespace cps;
	
	const auto func = pure(5) >> [=](auto a) {
		return pure(10) >> [=](auto b) {
			return pure(a + b);
		};
	};


	const auto func2 = yield() >> [=](int a) {
		std::clog << "a: " << a << std::endl;
		const int aa = a;
		return yield() >> [=](int b) {
			std::clog << "b: " << b << ", a: " << aa << std::endl;			
			return pure(aa + b);
		};
	};

	
	const auto show = [](const auto& x) {
		std::clog << "show: " << x << std::endl;
	};

	func(show);

	auto k = func2(show);
	
	k(4)(5);
	

	
	return 0;
}
