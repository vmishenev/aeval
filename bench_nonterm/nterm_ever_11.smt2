(declare-rel inv (Int Int))
(declare-var j Int)
(declare-var j1 Int)
(declare-var d Int)
(declare-var d1 Int)

(rule (inv 0 d))

(rule (=> 
    (and 
        (inv j d)
        (< d 50)
        (= d1 (mod j 50))
        (= j1 (+ j 1))
    )
    (inv j1 d1)
  )
)