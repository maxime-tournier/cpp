

(module (foo 'a)
        (attr (-> 'a integer)))

(module bar
        (attr (-> 'b integer)))

(def one (lambda (x) 1))

;; (export foo (record (test one)))
(export bar (record (attr one)))

;; (def test (lambda (f)
;;             (export bar (record (attr f)))))

;; generalization error
;; (test one)


(def use-bar (lambda ( (bar x) )
               (do
                (pure (@attr x true))
                (pure (@attr x 2.0)))))

;; type error
;; (def use-foo (lambda ( (foo x) )
;;                (do
;;                 (pure (@attr x true))
;;                 (pure (@attr x 2.0)))))

            
      
    




