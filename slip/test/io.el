
(do
 (print "michel")
 (pure 14))

(do)

(do
 (var x (pure 3))
 (pure x))

;; make sure toplevel context does not get screwed
(do
 (var x (pure 3))
 (pure x))



