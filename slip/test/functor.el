
(def map (lambda (x f)
     (cond ((nil? x) nil)
           (true (cons (f (head x)) (map (tail x) f))))))

(module (functor 'f)
        (map (-> ('f 'a) (-> (-> 'a 'b) ('f 'b)))))

;; export functor module
(export functor
        (record
         (map map)))

(def fmap (lambda ((functor f))
            (@map f)))
fmap


                
