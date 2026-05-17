Gyroscope : MultiOutUGen {
	*kr {
		^this.multiNew('control')
	}
	init { arg ... theInputs;
		inputs = theInputs;
		^this.initOutputs(4, rate);
	}
}

Accelerometer : MultiOutUGen {
	*kr {
		^this.multiNew('control')
	}
	init { arg ... theInputs;
		inputs = theInputs;
		^this.initOutputs(4, rate);
	}
}
