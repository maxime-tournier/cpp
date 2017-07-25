(def x (record
        (foo 1)
        (marcel "derp")
        (bar 2)))

(@marcel x)

(def foo-bar (lambda (x p)
               (cond (p (@foo x))
                     (true (@bar x)))))

(foo-bar x false)
                        

