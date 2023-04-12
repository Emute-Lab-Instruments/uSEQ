//quick serial interface for talking to useq
//any block of SC code starting with //u will get send to the module instead of evaluated in SC
//if this crashes, do Language->Reboot Interpreter

SerialPort.listDevices
//open a connection to the module
u = SerialPort.new('/dev/ttyACM0', baudrate:115200,databits:8, stopbit:false, parity:nil);


(
this.preProcessor = {
	|code|
	(code[..3] == "(//u").if({
		var cmd;
		">> useq >> ".post;
		cmd = code[4..code.size-2];
		u.putAll(cmd);
		cmd.post;
		code = "\"sent\""
	});
	code
};
Tdef(\serialReader, {
	var byte;
	var line="";
	loop({
		byte=u.next;
		(byte!=nil).if({
			line = "";
			while({byte != nil and: byte!=10},
				{
					line = line ++ byte.asAscii;
					byte=u.next;
			});
			(line != "").if({
				("useq> " ++ line).postln;
			});
		});
		1e-2.wait;
	});
}).play;
)


u.putAll("time")
u.putAll("(+ 120.4 10)")

(//u
(pm 25 1)
)

(//u
(dw 25 1)
)

(//u
(d2 (sqr (fast 4 beat)))
)

(//u
(d2 (* (fromList (quote 1 0 1 0 1 1 0 0) (fast 2 bar)) (sqr (fast 16 bar))))
)

(//u
(d2 (gates (quote 1 1 0 1  1 1 1 0  1 1 1 1  1 0 1 1) 1 0.9 ))
)

(//u
(d2 (gates (quote 1 1 0 1  1 1 1 0  1 1 1 1  1 0) 1 0.9 ))
)

(//u
(d2 (gates (quote 1 0 0 1  1 1 1 0  1 1 1 1  1 0 1 1) 1 0.8 ))
)

(//u
(a2 (+ (pulse (fast 2 bar) 0.9) 0.01))
)
(//u
(mdo 36 (lambda (t) (gates (quote 9 0 0 1) t (+ 2 (sqr t)))))
(mdo 43 (lambda (t) (gates (quote 9 0 1 0 9 0) t 4)))
)

(//u
(mdo 36 (lambda (t) (gates (quote 9 0 9 0) t (+ 2 (sqr t)))))
(mdo 43 (lambda (t) (gates (quote 9 0 1 0 1 0) t 4)))
)
(//u
(mdo 39 (lambda (t) (gates (quote 9 0 9 0 9 0 1 1) t 2)))
)
(//u
(mdo 39 (lambda (t) 0))
)
(//u
(perf)
)

//free the port
SerialPort.closeAll


