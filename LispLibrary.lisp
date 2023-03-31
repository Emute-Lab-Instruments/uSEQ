;; Test stuff, TODO delete
(defun led-on () (digitalWrite 25 1))
(defun led-off () (digitalWrite 25 0))
(defun blink-for (amt) (do (led-on) (delay amt) (led-off)))
(defun test-loop-1 () (loop (blink-for 200) (delay 100)))
(defun test-loop-2 () (loop (blink-for 800) (delay 400)))
(defun test-loop-3 () (loop (blink-for 500) (delay 50)))

;; FIXME not that important but should figure it out at some point
;; the first time it runs okay, but on subsequent calls it seems
;; to only blink once or twice before it crashes...
;; (only happens when passed to runOnThread)
;; Misc Utils

;;;; Timing API

;; Vars (mostly static)
;; TODO use meter in calculations, for now assumed 4/4
(define meter '(4 4))
(define bpm 120)
(define bps 2)
(define beatDur 500)
(define barDur 2000)
(define last-reset-time (millis))  ;; holds the last time the  transport  was reset
;; Useful phasors
(define time 0)
(define t 0)
(define beat 0)
(define bar 0)
;; Update functions
(defun bpm-to-bps (bpm) (/ bpm 60))
(defun bpm-to-beatDur (bpm) (/ 1000.0 (bpm-to-bps bpm)))
(defun bpm-to-barDur (bpm) (* (bpm-to-beatDur bpm) (first meter)))

(defun set-bpm (new-bpm)
  (do
   (set bpm new-bpm)
   (set bps (bpm-to-bps new-bpm))
    (set beatDur (bpm-to-beatDur new-bpm))
    (set barDur (bpm-to-barDur new-bpm))))

;; will be called once at the beginning of every loop
(defun set-time (new-time)
  (do
   (set time new-time)
   (set t (- new-time last-reset-time))
    (set beat (/ (% t beatDur) beatDur))
    (set bar (/ (% t barDur) barDur))))

(defun update-time ()
  (set-time (millis)))

(defun reset-time ()
  (do
   (set last-reset-time (millis))
   (set-time time)))
;; UI functions
(defun %1 (x) (% x 1.0))

(defun fast (amt phasor) (%1 (* phasor amt)))

;; TODO figure out if Wisp's `defun` can have &optionals
;; (defun pulse (on-relative-dur phasor &optional speed-mult)
;;   (let ((phasor (if speed-mult
;;                     (fast speed-mult phasor)
;;                     phasor)))
;;     (if (< phasor on-relative-dur) 1 0)))

;; (defun sqr (phasor &optional speed-mult)
;;   (pulse 0.5 phasor speed-mult))

;; ;; add (cos)sine
;; ;; add pow/log
;; (defun impulse (phasor &optional speed-mult)
;;   (pulse 0.05 phasor speed-mult))

;; (defun saw (phasor &optional fast-amt)
;;   (if fast-amt (fast fast-amt phasor) phasor))

;; (defun ramp (phasor &optional fast-amt)
;;   (saw phasor fast-amt))

(defun pulse (on-relative-dur phasor)
  (do
   (define phasor phasor)
   (if (< phasor on-relative-dur)
       1
       0)))

(defun sqr (phasor)
  (pulse 0.5 phasor))

;; add (cos)sine
;; add pow/log
(defun impulse (phasor)
  (pulse 0.05 phasor))

(defun saw (phasor)
  phasor)

(defun ramp (phasor)
  (saw phasor))

(defun clamp (mn mx x) (min mx (max x mn)))
(defun clamp01 (x) (clamp 0.0 1.0 x))

(defun add (amt x) (+ amt x))
(defun sub (amt x) (- amt x))
(defun mul (amt x) (* amt x))
(defun div (amt x) (/ amt x))

(defun index (l n)
    ; get the head of the result
    (head
        ; get the nth tail of the list `l`
        (reduce
            (lambda (acc arg) (tail acc))
            l
            (range 0 n))))

(defun nth (l n)
  (index l n))

(defun fromList (lst phasor)
  (do
   (define num-elements (length lst))
   (define scaled-phasor (* num-elements (clamp01 phasor)))
    (define idx (floor scaled-phasor))
    (nth lst
         (if (= idx num-elements)
             (- idx 1)
             idx))))

;; Outputs

(define INPUT_1_VALUE 0)
(define INPUT_2_VALUE 0)

;; for testing
(define led-form '(sqr beat))
(defun update-led () (c_digitalWrite 99 (eval led-form)))  ;; runs on core 1
(defun led (new-form) (set led-form new-form))  ;; runs on core 0

;; Digital outs
(define d1-form '(sqr beat))
(define d2-form '(sqr (fast 2 beat)))
(define d3-form '(sqr (fast 3 beat)))
(define d4-form '(sqr (fast 4 beat)))
(defun update-d1 () (dw 1 (eval d1-form)))
(defun update-d2 () (dw 2 (eval d2-form)))
(defun update-d3 () (dw 3 (eval d3-form)))
(defun update-d4 () (dw 4 (eval d4-form)))
(defun d1 (new-form) (set d1-form new-form))
(defun d2 (new-form) (set d2-form new-form))
(defun d3 (new-form) (set d3-form new-form))
(defun d4 (new-form) (set d4-form new-form))

;; Analog outs
(defun update-a1 () (c_analogWrite 1 (eval a1-form)))  ;; runs on core 1
(defun update-a2 () (c_analogWrite 2 (eval a2-form)))  ;; runs on core 1

;; TODO some kind of thread synchronisation needed here?
(defun in1 (new-form) INPUT_1_VALUE)
(defun in2 (new-form) (set d4-form new-form))  ;; runs on core 0

;; TODO
(defun useq_update ()
  (do
   (set-time (millis))
   (update-led)))
