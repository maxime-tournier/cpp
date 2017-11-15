

(module (foo 'a)
        (test (-> 'a integer)))

(def f (lambda ( (foo x) )
         x))
         ;; (@test x)))
f

(module bar
        (test (-> 'b integer)))

(def g (lambda ( (bar x) )
         x))
g

(def one (lambda (x) 1))

(export foo (record (test one)))
(export bar (record (test one)))

;; (def test (lambda (f)
;;             (export bar (record (test f)))))

;; (test (lambda (x) 1))





