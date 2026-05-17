WasmPlatform : UnixPlatform {
	name { ^"wasm".asSymbol; }
	version { ^"wasm" }

	startup {
		helpDir = this.systemAppSupportDir++"/Help";

		// Server setup. first looks for scsynth in the dir containing the sclang executable;
		// if nothing is found, falls back to PATH
		Server.program = "PATH=$(dirname $(readlink /proc/$PPID/exe)):$PATH; exec scsynth";

		// Score setup
		Score.program = Server.program;

		// load user startup file
		this.loadStartupFiles;
	}

	initPlatform {
		super.initPlatform;
	}

	killProcessByID { |pid, force = true, subprocesses = true|
		"killProcessById is not implemented".warn;
	}

	*activateSensors {
		^JS.runCode("scsynth.activateSensors()");
	}
}

JS {
	*runCode {|code|
		^this.prRunCode(code);
	}

	*prRunCode {|code|
		_Wasm_runCode
		^this.primitiveFailed;
	}
}
