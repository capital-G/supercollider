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

#pragma once
#include <iostream>
#include <ostream>

#include "PyrLexer.h"
#include "PyrObject.h"
#include "SC_LanguageClient.h"
#include <emscripten/eventloop.h>

#include "PyrSched.h"

// forward declaration
void wasmTick(void* arg);

/** @brief Implements a language client which redirects and exposes
 *  all I/O to a JavaScript interface via emscripten.
 */
class SC_WasmClient : public SC_LanguageClient {
public:
    SC_WasmClient(char const* name): SC_LanguageClient(name) {}

    virtual void postText(const char* str, size_t len) override {
        auto toPost = std::string(str, len);
        std::cout << toPost;
    }

    virtual void postFlush(const char* str, size_t len) override {
        auto toFlush = std::string(str, len);
        std::cout << toFlush;
    }

    virtual void postError(const char* str, size_t len) override {
        auto toError = std::string(str, len);
        std::cout << "error" << toError << std::endl;
    }

    virtual void flush() override { std::cout << std::endl; }

    virtual void onLibraryStartup() override;

    void runCode(std::string const& code) {
        setCmdLine(code.c_str());
        runLibrary(s_interpretPrintCmdLine);
    }

    int run(int argc, char** argv) override {
        auto options = Options();
        initRuntime(options);

        compileLibrary(false);

        if (!compiledOK) {
            post("ERROR: Library has not been compiled successfully.\n");
            return 1;
        }
        runMain();

        return 0;
    }

    // from SC_TerminalClient
    // instead of using boost asio, we are using JS callbacks
    void ticker();

    /** @brief responds to _AppClock_SchedNotify primitive */
    static int primitiveTicker(VMGlobals* g, int numArgsPushed);
};
