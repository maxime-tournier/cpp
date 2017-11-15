
# done

- basic hindley-milner calculus

- vm with function currying 
[see](https://www.microsoft.com/en-us/research/publication/make-fast-curry-pushenter-vs-evalapply/)

- row polymorphism à la [Daan
  Leijen](https://www.microsoft.com/en-us/research/publication/extensible-records-with-scoped-labels/)
  - actual label scoping not implemented (yet)
  - run-time attribute selection using chinese remainders 
    - (TODO not really tractable when size > 8-9)

- first-class polymorphism à la FCP [Jones97](http://web.cecs.pdx.edu/~mpj/pubs/popl97-fcp.pdf)
  - except only on record types
  - `module` defines record types with polymorphic variants (prenex, universally quantified):
    ```scheme
    (module (foo 'a)
        (attr (-> 'a integer)))

    (module bar
        (attr (-> 'b integer)))
    ```
  - `export` acts like a data constructor + generalization checks:
    ```scheme
    (export bar (record (attr (lambda (x) 1)))
    ```

  - lambda parameters may be explicitly annotated to open modules as regular
    record types:
    ```scheme
    (def use-bar (lambda ( (bar x) )
       (do
         (pure (@attr x true))
         (pure (@attr x 2.0)))))
    ``` 

# needed

- currying: store argc for builtin functions

- algebraic datatypes + pattern matching?
 
- runst-like monad escape (done with ad-hoc typing rules, but it should be
  encodable using FCP)

- mutable refs & records? or monadic?
  
- better record attribute selection mechanism + separate compilation?
  - find a way to hash symbols with prime numbers globally
  - fix minimum record size?

# planned

- typeclasses
  - need higher-kinded polymorphism for monads

- automatic coercion when passing modules around in functions (using typeclasses?)

- better vm/llvm/???

# lolwat

- existentials? 
  - could row-polymorphism be used instead to package interfaces?

- lifetime inference/ownership tracing

- macros?




    
