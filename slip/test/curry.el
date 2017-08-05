

;; over-saturated calls
(def over1 (lambda (x) (lambda (y) (lambda (z) (+ z (+ x y))))))
(def over2 (lambda (x) (lambda (y z) (+ z (+ x y)))))
(def over3 (lambda (x y) (lambda (z) (+ z (+ x y)))))

(over1 1 2 3)
(over2 1 2 3)
(over3 1 2 3)

; partial applications
(def add (lambda (x y z) (+ z(+ x y))))
(def add1 (add 1))
(def add12 (add 1 2))

((add1 2) 3)
(add12 3)




