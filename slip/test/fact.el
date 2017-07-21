(def fact (lambda (n)
			(cond ((= n 0) 1)
				  (true (* n (fact (- n 1)))))))

(fact 5)
