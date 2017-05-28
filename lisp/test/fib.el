
(def fib (lambda (n)
           (cond ((eq n 0) 0)
                 ((eq n 1) 1)
                 ('else (add (fib (sub n 1))
                             (fib (sub n 2)))))))
(fib 30)


