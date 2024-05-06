;;;; List Functions
(defun index (l n)
  (head
   (reduce
    (lambda (acc arg) (tail acc))
    l
    (range 0 n))))

(defun first (l) (index l 0))
(defun second (l) (index l 1))
(defun third (l) (index l 2))

(defun nth (l n)
  (index l n))

(defun %1 (x) (% x 1.0))



(defun impulse (phasor)
  (pulse 0.05 phasor))

(defun saw (phasor)
  phasor)

(defun ramp (phasor)
  (saw phasor))

(defun clamp (mn mx x) (min mx (max x mn)))
(defun clamp01 (x) (clamp 0.0 1.0 x))

(defun every (amt dur) (/ (% t (* amt dur)) (* amt dur)))


;;(defun fromList (lst phasor)
;;  (do
;;   (define num-elements (len lst))
;;   (define scaled-phasor (* num-elements (clamp01 phasor)))
;;    (define idx (floor scaled-phasor))
;;    (nth lst
;;         (if (= idx num-elements)
;;             (- idx 1)
;;             idx))))

;; Outputs

(define q-form 0)

;;(defun sig-in (index) (useqGetInput index))
;;(defun in1 () (useqGetInput 1))
;;(defun in2 () (useqGetInput 2))
;;(defun swm (index) (useqGetInput (add 2 index)))
;;(defun swt (index) (useqGetInput (add 4 index)))
;;(defun swr () (useqGetInput 6))
;;(defun rot () (useqGetInput 7))

(defun seq (lst speed)  (fromList lst (every speed beatDur)))

;;(defun gates (lst ph speed pw) (* (fromList lst (fast speed ph)) (pulse (fast (* speed (len lst)) ph) pw)))
