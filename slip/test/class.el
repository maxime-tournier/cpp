

(def make-foo
     (lambda (x y z)
       (do
        (def self (record
                   (x x)
                   (y y)
                   (z z) ))
        (pure
         (record
          (add-x (lambda (arg) (+ (@x self) arg)))
          (add-y (lambda (arg) (+ (@y self) arg)))
          )))))

(do
 (var foo (make-foo 8 2 3))
 (pure (@add-x foo 2))
 ;; (pure (@add-y foo 5)) 
 )
;; ((@add-x foo) 2)
