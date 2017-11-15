

(module (foo 'a)
        (attr (-> 'a integer)))

(module bar
        (attr (-> 'b integer)))

(def one (lambda (x) 1))

;; (export foo (record (test one)))
(export bar (record (attr2 one)))

(def test (lambda (f)
            (export bar (record (attr f)))))

(test one)





