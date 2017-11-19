
(def map (lambda (x f)
     (cond ((nil? x) nil)
           (true (cons (f (head x)) (map (tail x) f))))))

(module (functor 'f)
        (map (-> ('f 'a) (-> (-> 'a 'b) ('f 'b)))))

;; export functor module
(def f (export functor
               (record
                (map map))))
f

(def open (lambda ((functor f))
            f))
open

(def fmap (lambda ((functor f))
            (@map f)))
fmap

                
