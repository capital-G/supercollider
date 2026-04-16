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
#include <SC_Filesystem.hpp>
#include <SC_LanguageConfig.hpp>
#include <emscripten/bind.h>
#include <emscripten/emscripten.h>
#include <emscripten/threading.h>

#include "PyrPrimitive.h"
#include "SC_ComPort.h"
#include "SC_World.h"
#include "VMGlobals.h"
#include "SC_Msg.h"

// the client is stored as global variable, making it a singleton
static SC_WasmClient* gWasmClient = nullptr;
// language does run in its own thread
static pthread_t gSclangWasmThread;

/** @brief runs JS code on the main browser thread where window/document exist **/
static void runJsOnMainThread(void* arg) {
    char* code = static_cast<char*>(arg);
    emscripten_run_script(code);
    free(code);
}

/** @brief a sclang primitive to run code in the js main thread */
static int prRunJsCode(struct VMGlobals* g, int numArgsPushed) {
    auto [err, code] = slotStdStrVal(g->sp);
    if (err != errNone) {
        return err;
    }
    char* codeCopy = strdup(code.c_str());
    emscripten_async_run_in_main_runtime_thread(EM_FUNC_SIG_VI, runJsOnMainThread, codeCopy);

    return errNone;
}


void SC_WasmClient::onLibraryStartup() {
    SC_LanguageClient::onLibraryStartup();
    int base, index = 0;
    base = nextPrimitiveIndex();
    definePrimitive(base, index++, "_Wasm_runCode", prRunJsCode, 2, 0);
    definePrimitive(base, index++, "_AppClock_SchedNotify", primitiveTicker, 1, 0);
}

void SC_WasmClient::ticker() {
    double secs;
    lock();
    bool haveNext = tickLocked(&secs);
    unlock();

    if (haveNext) {
        double now = elapsedTime();
        double delayMs = (secs - now) * 1000.0;
        // lower bound clip to avoid suffocation of JS thread(?)
        if (delayMs < 1.0) {
            delayMs = 1.0;
        }
        emscripten_set_timeout(wasmTick, delayMs, nullptr);
    }
}

int SC_WasmClient::primitiveTicker(VMGlobals* g, int numArgsPushed) {
    // defer execution to js runtime
    emscripten_set_timeout(wasmTick, 1, nullptr);
    return errNone;
};

void wasmTick(void*) { gWasmClient->ticker(); }

static void* bootInterpreter(void* args) {
    gWasmClient = new SC_WasmClient("sclang");
    std::cout << "Welcome to sclang.wasm!" << std::endl;
    if (!gWasmClient) {
        std::cout << "ERROR: Failed to create sclang client." << std::endl;
        return nullptr;
    };

    // add quark import dir
    gLanguageConfig = new SC_LanguageConfig();
    gLanguageConfig->setExcludeDefaultPaths(false);
    auto quarkDir =
        SC_Filesystem::instance().getDirectory(SC_Filesystem::DirName::UserAppSupport).append("downloaded-quarks");
    gLanguageConfig->addIncludedDirectory(quarkDir);

    auto options = SC_LanguageClient::Options();
    gWasmClient->initRuntime(options);
    compileLibrary(false);

    if (!compiledOK) {
        std::cout << "ERROR: Library has not been compiled successfully." << std::endl;
        return nullptr;
    }
    gWasmClient->runMain();
    emscripten_exit_with_live_runtime();
};

void executeCode(void* arg) {
    char* code = static_cast<char*>(arg);
    gWasmClient->runCode(code);
    free(code);
}

void runCodeOnSclangThread(const std::string& code) {
    char* codeCopy = strdup(code.c_str());
    emscripten_dispatch_to_thread_async(gSclangWasmThread, EM_FUNC_SIG_VI, executeCode, nullptr, codeCopy);
}

void runCode(std::string code) {
    if (!gWasmClient) {
        std::cout << "wasm client not initialized!" << std::endl;
        return;
    }
    gWasmClient->runCode(code);
}


void ProcessOSCPacket(std::unique_ptr<OSC_Packet> inPacket, int inPortNum, double time);

static void runOscMessage(void* arg) {
    auto* packet = static_cast<OSC_Packet*>(arg);
    ProcessOSCPacket(std::unique_ptr<OSC_Packet>(packet), 57120, elapsedTime());
}

void passOscMessageToSclangThread(std::string data) {
    if (!gWasmClient) {
        std::cout << "wasm client not initialized!" << std::endl;
        return;
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

    emscripten_dispatch_to_thread_async(gSclangWasmThread, EM_FUNC_SIG_VI, runOscMessage, nullptr, packet);
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
    std::memcpy(rawMessage, bufptr, msglen);
    // pass message to main thread
    MAIN_THREAD_ASYNC_EM_ASM(
        {
            if (Module.onOsc) {
                var data = HEAPU8.slice($0, $0 + $1);
                Module.onOsc(data);
                Module['_free']($0);
            }
        },
        rawMessage, msglen);
    return errNone;
}

// js export

void cBootInterpreter() {
    if (gWasmClient != nullptr) {
        std::cout << "sclang already running" << std::endl;
        return;
    }
    pthread_create(&gSclangWasmThread, nullptr, bootInterpreter, nullptr);
}

EMSCRIPTEN_BINDINGS(sclangWasm) {
    emscripten::function("bootInterpreter", &cBootInterpreter);
    emscripten::function("runCode", &runCodeOnSclangThread);
    emscripten::function("sendOsc", &passOscMessageToSclangThread);
}

// export this to avoid dead code elimination
EMSCRIPTEN_KEEPALIVE extern "C" void scWasmBindingAnchor() {

};
