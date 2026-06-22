SpawnSynth : UGen {
	*ar {|name, trigger=0, nodeID=nil, addAction=1, target=1|
		^this.multiNew('audio', trigger, nodeID ? -1, addAction, target, *name.asSignal);
	}
}
