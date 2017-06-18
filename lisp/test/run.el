

;; (run
;;  (var x (ref 0))
;;  (get x))

(def z (run
        (var y (ref nil))
        (pure y)))
z

     


;; (do
;;  (var x (ref 0))
;;  (pure (run
;;         (get x))))

