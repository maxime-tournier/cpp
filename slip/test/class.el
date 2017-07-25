

(def make-foo
     (lambda (x y z)
       (do
        (def self (record
                   (x x)
                   (y y)
                   (z z) ))
        (pure
         (record
          (add-x (lambda (y) (+ (@x self) y))))))))

(do
 (var foo (make-foo 1 2 3))
 (pure (@add-x foo 2))
 ;; (pure foo)
 )
;; ((@add-x foo) 2)
