
(lambda (x)
  (do
   ;; y should not be generalized here
   (def y (cons nil x))

   ;; otherwise both are allowed
   (pure (cons (cons 1 nil) y))
   (pure (cons (cons "true" nil) y))   
  ))


(var x (ref nil))
(cons nil (get x))

