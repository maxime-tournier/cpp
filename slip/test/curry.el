

;; extra args on functions returning functions
(def over (lambda (x) (lambda (y) (+ x y))))
(over 1 2)

;; curryfied functions
(def add1 (+ 1))
(add1 2)


