

(module (monad 'm)
        (ret (-> 'a ('m 'a)))
        ;; (bind (-> ('m 'a) (-> (-> 'a ('m 'b))
        ;;                       ('m  'b))))
        )


(export monad
        (record 
         (ret (lambda (x) (cons x nil)))))
        
               
