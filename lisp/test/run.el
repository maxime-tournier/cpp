

(run
 (var x (ref 0))
 (var *x (get x))
 (pure *x)
 )



(var y (ref 0))
(run
 (var *y (get y))
 (pure *y))



;; (do
;;  (var x (ref 0))
;;  (pure (run
;;         (get x))))

