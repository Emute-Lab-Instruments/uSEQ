// LispLibrary.h
#pragma once
const String PROGMEM LispLibrary[] = {
"(defun index (l n)\n  (head\n   (reduce\n    (lambda (acc arg) (tail acc))\n    l\n    (range 0 n))))",
"(defun first (l) (index l 0))",
"(defun second (l) (index l 1))",
"(defun third (l) (index l 2))",
"(defun nth (l n)\n  (index l n))",
"(defun %1 (x) (% x 1.0))",
"(defun impulse (phasor)\n  (pulse 0.05 phasor))",
"(defun saw (phasor)\n  phasor)",
"(defun ramp (phasor)\n  (saw phasor))",
"(defun clamp (mn mx x) (min mx (max x mn)))",
"(defun clamp01 (x) (clamp 0.0 1.0 x))",
"(defun every (amt dur) (/ (% t (* amt dur)) (* amt dur)))",
"(define q-form 0)",
"(defun seq (lst speed)  (fromList lst (every speed beatDur)))"
};

const int LispLibrarySize = 14;
