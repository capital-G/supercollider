/*
SuperCollider real time audio synthesis system wasm binding
    Copyright (c) 2026 Dennis Scheiba. All rights reserved.
    https://supercollider.github.io/

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "SC_WasmClient.h"

#include <iostream>
#include <emscripten/bind.h>
#include <emscripten/emscripten.h>
#include <emscripten/threading.h>

#include "PyrKernel.h"
#include "PyrPrimitive.h"
#include "PyrSched.h"
#include "SC_ComPort.h"
#include "SC_World.h"
#include "VMGlobals.h"
#include "SC_Msg.h"

enum class InterpreterStatus {
    Idle,
    Booting,
    CompilationFailed,
    Running,
};

// language does run in its own thread
static pthread_t gSclangWasmWorkerThread;

// interpreter status are written from the sclang thread, but needs to be available from js side
// so we need to use a mutex to avoid memory scramble ;)
static std::mutex gInterpreterStatusMutex;
static InterpreterStatus gInterpreterStatus = InterpreterStatus::Idle;

// forward declaration
void wasmTick(void* arg);

/** @brief runs JS code on the main browser thread where window/document exist
 *
 *  @param arg takes ownership by freeing the char* after execution.
 * **/
static void runJsOnMainThread(void* arg) {
    char* code = static_cast<char*>(arg);
    emscripten_run_script(code);
    free(code);
}

/** @brief a sclang primitive to run code in the js main thread */
static int prRunJsCode(struct VMGlobals* g, int numArgsPushed) {
    bool isString = g->sp->getObjectHdr() && g->sp->getClass() == class_string;
    if (!isString) {
        return errWrongType;
    }
    auto code = g->sp->getPyrObjType<PyrString>();

    // transfer ownership of the code copy to runJsOnMainThread to avoid lifetime issues
    emscripten_async_run_in_main_runtime_thread(EM_FUNC_SIG_VI, runJsOnMainThread, strndup(code->s, code->size));

    return errNone;
}

/** @brief sends a raw OSC byte array to the IDE. This will not be forwarded to the server.  */
static int prIdeSend(struct VMGlobals* g, int numArgsPushed) {
    if (!isKindOfSlot(g->sp, class_int8array)) {
        return errWrongType;
    }
    PyrInt8Array* array = slotRawInt8Array(g->sp);
    int size = array->size;
    char* copy = static_cast<char*>(malloc(size));
    std::memcpy(copy, array->b, size);
    MAIN_THREAD_ASYNC_EM_ASM(
        {
            try {
                if (Module['onIdeSend']) {
                    Module['onIdeSend'](HEAPU8.slice($0, $0 + $1));
                }
            } finally { Module['_free']($0); }
        },
        copy, size);
    return errNone;
}

/** @brief responds to _AppClock_SchedNotify primitive */
static int prAppClockSchedNotify(VMGlobals* g, int numArgsPushed) {
    // defer execution to run in the gSclangWasmThread event loop
    emscripten_dispatch_to_thread_async(gSclangWasmWorkerThread, EM_FUNC_SIG_VI, &wasmTick, nullptr,
                                        // for some reason we need an excessive parameter here
                                        // using a plain `EM_FUNC_SIG_V` results in a compile error
                                        nullptr);
    return errNone;
}

void SC_WasmClient::onLibraryStartup() {
    SC_LanguageClient::onLibraryStartup();
    int index = 0;
    int base = nextPrimitiveIndex();
    definePrimitive(base, index++, "_Wasm_runCode", prRunJsCode, 2, 0);
    definePrimitive(base, index++, "_Wasm_ideSend", prIdeSend, 2, 0);
    definePrimitive(base, index++, "_AppClock_SchedNotify", prAppClockSchedNotify, 1, 0);
}

void SC_WasmClient::runCode(const std::string& code, const bool silent) {
    setCmdLine(code.c_str());
    runLibrary(silent ? s_interpretCmdLine : s_interpretPrintCmdLine);
}

void SC_WasmClient::scheduleTick(double delayMs) {
    // std::chrono::high_resolution_clock is nano seconds in emscripten
    const double deadline = elapsedTime() + delayMs * 0.001;
    // there is already a timeout which will invoke us earlier - so bail out
    if (mTickTimeoutId != 0 && mTickDeadline <= deadline) {
        return;
    }
    if (mTickTimeoutId != 0) {
        // we can clear the existing timer since we will create a new one
        // which will wake us up sooner than the existing one
        emscripten_clear_timeout(mTickTimeoutId);
    }
    mTickTimeoutId = emscripten_set_timeout(wasmTick, delayMs, nullptr);
    mTickDeadline = deadline;
}


void SC_WasmClient::ticker() {
    // the pending timeout that called us has timed out
    mTickTimeoutId = 0;
    double secs;
    lock();
    const bool haveNext = tickLocked(&secs);
    unlock();

    if (haveNext) {
        double now = elapsedTime();
        double delayMs = (secs - now) * 1000.0;
        // lower bound clip to give others room to breath
        if (delayMs < 1.0) {
            delayMs = 1.0;
        }
        if (!isnan(delayMs) && !isinf(delayMs)) {
            scheduleTick(delayMs);
        } else {
            std::cout << "Invalid delayMs value for AppClock ticker: " << delayMs << std::endl;
        }
    }
}

/**
 * This C function will be invoked by a JS timeout which runs on the gSclangWasmThread.
 */
void wasmTick(void*) {
    auto client = static_cast<SC_WasmClient*>(SC_WasmClient::instance());
    // this can never be null b/c we only get called from within a primitive
    // or at client init
    assert(client != nullptr);
    client->ticker();
}

void SC_WasmClient::postText(const char* str, size_t len) { std::cout.write(str, len); }

void SC_WasmClient::postFlush(const char* str, size_t len) {
    std::cout.write(str, len);
    std::cout.flush();
}

void SC_WasmClient::postError(const char* str, size_t len) {
    std::cout << "error";
    std::cout.write(str, len);
    std::cout << std::endl;
}

void SC_WasmClient::flush() { std::cout << std::endl; }

/** @brief Called as entry point by the dedicated gSclangWasmThread, which will boot the interpreter.
 */
static void* wasmWorkerThreadFunction(void* args) {
    auto client = SC_WasmClient("sclang");
    std::cout << "Welcome to sclang.wasm!" << std::endl;
    SC_LanguageClient::Options options;
    client.initRuntime(options);
    const auto compileSuccess = compileLibrary(false);
    {
        std::lock_guard lock(gInterpreterStatusMutex);
        if (!compileSuccess) {
            gInterpreterStatus = InterpreterStatus::CompilationFailed;
            std::cout << "ERROR: Library has not been compiled successfully." << std::endl;
            return nullptr;
        }
        gInterpreterStatus = InterpreterStatus::Running;
    }
    // this does not block
    client.runMain();
    // this not just keeps the owned resources "alive", but also
    // keeps the worker thread alive such that the JS runtime can
    // process events from e.g. the AppClock.
    emscripten_exit_with_live_runtime();
};

/**
 * @brief Evaluates code in the sclang interpreter.
 * This should only be called from the gSclangWasmThread in order to avoid deadlocks of the gLangMutex lock.
 *
 * @param arg the char* gets freed after execution
 */
void executeCode(void* arg, const bool silent) {
    char* code = static_cast<char*>(arg);

    // use a cache variable to hold the mutex as short as possible
    // as run code could lead to extended locking and maybe a deadlock
    bool interpreterRunning;
    {
        std::lock_guard lock(gInterpreterStatusMutex);
        interpreterRunning = gInterpreterStatus == InterpreterStatus::Running;
    }

    if (interpreterRunning) {
        auto client = static_cast<SC_WasmClient*>(SC_WasmClient::instance());
        // client can not be null here b/c we only set interpreter running
        // when the client was created
        assert(client != nullptr);
        client->runCode(code, silent);
    }
    free(code);
}


void runCodeOnSclangThread(const std::string& code, const bool silent = false) {
    char* codeCopy = strdup(code.c_str());
    emscripten_dispatch_to_thread_async(gSclangWasmWorkerThread, EM_FUNC_SIG_VII, executeCode, nullptr, codeCopy,
                                        silent);
}

// acts as overload - emscripten does not support default arguments, so we provide an indirection here
void runCodeSclangThreadLoud(const std::string& code) { runCodeOnSclangThread(code, false); }


void ProcessOSCPacket(std::unique_ptr<OSC_Packet> inPacket, int inPortNum, double time);

/**
 * @param arg must point to an OSC_Packet* - also takes ownership. */
static void runOscMessage(void* arg) {
    auto* packet = static_cast<OSC_Packet*>(arg);
    ProcessOSCPacket(std::unique_ptr<OSC_Packet>(packet), 57120, elapsedTime());
}

void passOscMessageToSclangThread(std::string data) {
    {
        std::lock_guard lock(gInterpreterStatusMutex);
        if (gInterpreterStatus != InterpreterStatus::Running) {
            std::cout << "sclang client not running!" << std::endl;
            return;
        }
    }
    // data contains raw OSC bytes (embind copies Uint8Array into std::string)
    // build the packet on this thread, copy the bytes, dispatch to sclang thread
    // data is not leaking b/c ownership gets passed to the sclang thread
    auto packet = new OSC_Packet();
    packet->mData = std::make_unique<char[]>(data.size());
    std::memcpy(packet->mData.get(), data.data(), data.size());
    packet->mSize = data.size();
    packet->mReplyAddr.mPort = 57110;
    packet->mReplyAddr.mProtocol = kUDP;
    packet->mReplyAddr.mReplyData = nullptr;
    packet->mReplyAddr.mSocket = 12345;

    emscripten_dispatch_to_thread_async(gSclangWasmWorkerThread, EM_FUNC_SIG_VI, runOscMessage, nullptr, packet);
}

// patches
void startAsioThread() {}
void stopAsioThread() {}
void initSerialPrimitives() {}

InPort::UDP::UDP(int inPortNum, HandlerType, int portsToCheck) {}
InPort::UDPCustom::UDPCustom(int inPortNum, HandlerType handlerType): UDP(inPortNum, handlerType, 1) {}
OutPort::TCP::TCP(std::uint64_t inAddress, int inPort, HandlerType, ClientNotifyFunc notifyFunc, void* clientData) {}
int OutPort::TCP::Close() { return errNone; }

SCSYNTH_DLLEXPORT_C bool World_SendPacket(World* inWorld, int inSize, char* inData, ReplyFunc inFunc) { return true; }
SCSYNTH_DLLEXPORT_C bool World_SendPacketWithContext(World* inWorld, int inSize, char* inData, ReplyFunc inFunc,
                                                     void* inContext) {
    return true;
}

// OSCData.cpp / network patches

/** @brief Module.onOsc needs to be called on the main JS thread.
 *  This helper gets passed a raw pointer to an OSC message,
 *  which then invokes onOsc using this main JS thread.
 *  The passed message will be freed.
 *
 *  See https://emscripten.org/docs/porting/pthreads.html#proxying
 *  and https://emscripten.org/docs/api_reference/proxying.h.html
 */
int netAddrSend(PyrObject* netAddrObj, int msglen, char* bufptr, bool sendMsgLen) {
    // prepend size of the message
    char* rawMessage = static_cast<char*>(malloc(msglen));
    if (!rawMessage) {
        post("NetAddr: Could not allocate memory for OSC message");
        return errFailed;
    }
    std::memcpy(rawMessage, bufptr, msglen);
    // pass message to main thread
    MAIN_THREAD_ASYNC_EM_ASM(
        {
            try {
                if (Module.onOsc) {
                    var data = HEAPU8.slice($0, $0 + $1);
                    Module.onOsc(data);
                }
            } finally { Module['_free']($0); }
        },
        rawMessage, msglen);
    return errNone;
}

// js export

void cBootInterpreter() {
    {
        std::lock_guard lock(gInterpreterStatusMutex);
        if (gInterpreterStatus != InterpreterStatus::Idle) {
            std::cout << "sclang already booted" << std::endl;
            return;
        }
        gInterpreterStatus = InterpreterStatus::Booting;
    }
    pthread_create(&gSclangWasmWorkerThread, nullptr, wasmWorkerThreadFunction, nullptr);
}

EMSCRIPTEN_BINDINGS(sclangWasm) {
    emscripten::function("bootInterpreter", &cBootInterpreter);
    // emscripten does not respect c++ default arguments
    // .runCode(code, silent)
    emscripten::function("runCode", &runCodeOnSclangThread);
    // .runCode(code) => .runCode(code, silent=false) in C++
    emscripten::function("runCode", &runCodeSclangThreadLoud);
    emscripten::function("sendOsc", &passOscMessageToSclangThread);
}

// export this to avoid dead code elimination
EMSCRIPTEN_KEEPALIVE extern "C" void scWasmBindingAnchor() {

};
