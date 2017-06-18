`slip` is a small ML-like language with a lispy syntax, written in c++11. It is
meant as a basis for experimentation with type systems. It has the following
features:

- a **repl**: 
  `$ ./slip`
  
- a small **bytecode compiler + vm**. you can see the resulting bytecode using: 
  `$ ./slip --dump`

- a few **primitive types**: `unit, boolean, integer, real, string, symbol`
- a few non-primitive types:
  - function types `'a -> 'b`
  - lists `list 'a`
  - mutable refs `ref 'a 'b`
  - computations `io 'a 'b`
  
- very few primitive functions so far (more will be added once typeclasses are
  in):
  - `+, -, /, *, %, =` for integers
  - `cons` for list
  - empty list is `nil : list 'a`

- **conditionals**: `(cond ((test1 value1) (test2 value2) ... )`
- **lambdas**: `(lambda (x) x) : 'a -> 'a`
- **definitions** (let-polymorphism) `(def id (lambda (x) x)) : io unit`

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
      
    - the language is strict: the monad is only for type-checking purpose
    
- **mutable references**
  - `ref : 'a -> io ref 'a 'b` where `'a` is the value type and `'b` is a
    phantom thread type

  - `get : ref 'a 'b -> io 'a 'b` dereferences a cell
  - `set : ref 'a 'b  -> 'a -> io unit 'b` mutates value

  it is expected that threading sequencing and mutation in such a monadic way
  will remove the need for ML's value restriction. since the monad is safely
  escapable, it should keep `io` from creeping up everywhere.

- there is also a small lisp interpreter that might be useful for a macro
  system
