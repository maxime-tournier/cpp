
(@x (record (x 1)))


(def use (lambda (ops x y)
		   (do
			(var z (pure (@+ ops x y)))
			(@print ops z))))
use


