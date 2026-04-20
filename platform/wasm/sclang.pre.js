// print methods are made get only, but we want them to be passed to the user,
// so we add dummy methods with postfix callback
Module['printCallback'] = function(text) {
    console.log(text);
};

Module['printErrCallback'] = function(text) {
    console.error(text);
};

Module['print'] = function(text) {
    Module['printCallback'](text);
};

Module['printErr'] = function(text) {
    Module['printErrCallback'](text);
};

Module['onRuntimeInitialized'] = function() {
    console.log("Spinning up sclang");
};

/**
 * Gets called from sclang when it sends out an OSC message.
 *
 * @param message {Uint8Array} This will be freed, so it is necessary to make a copy
 * if it should be passed around.
 */
Module['onOsc'] = function(message){
    console.log("New OSC message from sclang", message);
}
