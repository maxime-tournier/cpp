

(def map (lambda (x f)
     (cond ((nil? x) nil)
           (true (cons (f (head x)) (map (tail x) f))))))

(map (cons 1 (cons 2 (cons 3 nil)))
     (lambda (x) (+ 5 x)))

