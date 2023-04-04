//quick serial interface for talking to useq
//any block of SC code starting with //u will get send to the module instead of evaluated in SC
//if this crashes, do Language->Reboot Interpreter

SerialPort.listDevices
//open a connection to the module
u = SerialPort.new('/dev/ttyACM0', baudrate:115200)


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



(//u
(pm 25 1)
)

(//u
(dw 25 0)
)

(//u
(d1 (fast 2 bar))
)


//free the port
SerialPort.closeAll


