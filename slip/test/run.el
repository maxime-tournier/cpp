
;; this is ok
(run
 (var x (ref nil))
 (get x))


;; not ok: inner thread escapes
(run (ref nil))

;; not ok: inner thread references outer thread and maybe unpure
(var x (ref nil))
(run (get x))


