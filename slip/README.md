`slip` is a small ML-like language with a lispy syntax, written in c++11. It is
meant as a basis for experimentation with type systems. It has the following
features:

- a **repl**: 
  `$ ./slip`
  
- a small **bytecode compiler + vm**. you can see the resulting bytecode using: 
  `$ ./slip --dump`

- a few **primitive types**: `unit, boolean, integer, real, string`
- a few non-primitive types:
  - function types `'a -> 'b`
  - lists: `list 'a`
  - computations: `io 'a`
  - record types: `(record (foo 1) (bar true))` attribute selection: `(@foo x)`
  - modules (records with polymorphic attribute types, boxed behind rank1 types)
    `(module my-module (length (-> 'a integer)))`
  
- very few primitive functions so far (more will be added once typeclasses are
  in):
  - `+, -, /, *, %, =` for integers
  - `cons` for list
  - empty list: `nil : list 'a`

- **lambdas**: `(lambda (x) x) : 'a -> 'a`
- **definitions** (let-polymorphism) `(def id (lambda (x) x)) : io unit`



- **conditionals**: `(cond ((test1 value1) (test2 value2) ... )`

- **sequencing** is typechecked using a monad similar to haskell's `ST`:
    ```lisp
        (do
            (var x (pure 0))
            (print x))
    ```
    - `var` is monadic binding, `pure` is monadic return
    - all sequence computation have type `io 'a 'b`, where `'a` is the value
      type and `'b'` is a phantom type binding to the computation thread
      
    - the `io` monad is escapable using `run` provided computations don't
      reference outer threads and that the inner thread does not leak outside
      (similar to `runST` in haskell): `(run (var x (pure 0) ) (pure x))` has
      type `integer`
      
    - the language is strict: the `io` monad is only for type-checking purpose
