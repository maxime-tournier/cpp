
(def fib (lambda (n)
           (cond ((eq n 0) 1)
                 ((eq n 1) 1)
                 (1 (add (fib (sub n 1))
                         (fib (sub n 2)))))))
(print (fib 30))


