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

#include <string>
#include <emscripten.h>
#include <iostream>
#include <emscripten/bind.h>

#include "osc/OscOutboundPacketStream.h"
#include "osc/OscReceivedElements.h"
#include "osc/OscTypes.h"

constexpr int OUTPUT_BUFFER_SIZE = 8192;

/**
 * A class to construct OSC messages via wasm so that
 * we don't need another JavaScript library to construct
 * OSC messages.
 *
 * This mimics the osc-pack API but as it relies on operator
 * overloading we have to provide some glue code as embind/JavaScript
 * can not overload functions and operators.
 */
class OscMessageBuilder {
public:
    OscMessageBuilder(): mStream(buffer, OUTPUT_BUFFER_SIZE) {}

    OscMessageBuilder& beginMessage(std::string address) {
        mStream << osc::BeginMessage(address.c_str());
        return *this;
    }

    OscMessageBuilder& endMessage() {
        mStream << osc::EndMessage;
        return *this;
    }

    OscMessageBuilder& beginBundle() {
        mStream << osc::BeginBundle();
        return *this;
    }

    OscMessageBuilder& endBundle() {
        mStream << osc::EndBundle;
        return *this;
    }

    OscMessageBuilder& addBlob(const emscripten::val& uint8Array) {
        unsigned int length = uint8Array["length"].as<unsigned int>();

        // first transfer the byte array into a vec<byte>
        std::vector<uint8_t> byteArray;
        byteArray.resize(length);
        auto memory = emscripten::val::module_property("HEAPU8")["buffer"];
        auto memoryView = uint8Array["constructor"].new_(memory, reinterpret_cast<uintptr_t>(byteArray.data()), length);
        memoryView.call<void>("set", uint8Array);
        char* mBufPtr = reinterpret_cast<char*>(byteArray.data());

        mStream << osc::Blob(mBufPtr, length);

        return *this;
    }

    OscMessageBuilder& addInt(int value) {
        mStream << osc::int32(value);
        return *this;
    }

    OscMessageBuilder& addFloat(float value) {
        mStream << value;
        return *this;
    }

    OscMessageBuilder& addString(const std::string& value) {
        mStream << value.c_str();
        return *this;
    }

    emscripten::val getData() { return emscripten::val(emscripten::typed_memory_view(mStream.Size(), mStream.Data())); }

private:
    char buffer[OUTPUT_BUFFER_SIZE];
    osc::OutboundPacketStream mStream;
};

/**
 * Will be translated to an JS object by embind
 */
struct ParsedOscMessage {
    std::string address;
    emscripten::val arguments = emscripten::val::array();
};

/**
 * Parse OSC arguments from a ReceivedMessage into a JS array.
 */
static void parseOscArgs(const osc::ReceivedMessage& message, emscripten::val& arguments) {
    for (auto arg = message.ArgumentsBegin(); arg != message.ArgumentsEnd(); ++arg) {
        if (arg->IsInt32()) {
            arguments.call<void>("push", emscripten::val(arg->AsInt32Unchecked()));
        } else if (arg->IsFloat()) {
            arguments.call<void>("push", emscripten::val(arg->AsFloatUnchecked()));
        } else if (arg->IsString()) {
            arguments.call<void>("push", emscripten::val(arg->AsStringUnchecked()));
        } else if (arg->IsBlob()) {
            const void* blobData;
            osc::osc_bundle_element_size_t blobSize;
            arg->AsBlobUnchecked(blobData, blobSize);
            auto uint8Ctor = emscripten::val::global("Uint8Array");
            auto blob = uint8Ctor.new_(emscripten::val(blobSize));
            for (osc::osc_bundle_element_size_t i = 0; i < blobSize; ++i) {
                blob.set(i, emscripten::val(static_cast<const uint8_t*>(blobData)[i]));
            }
            arguments.call<void>("push", blob);
        } else if (arg->IsDouble()) {
            arguments.call<void>("push", emscripten::val(arg->AsDoubleUnchecked()));
        } else if (arg->IsBool()) {
            arguments.call<void>("push", emscripten::val(arg->AsBoolUnchecked()));
        } else if (arg->IsNil()) {
            arguments.call<void>("push", emscripten::val::null());
        } else {
            std::cout << "unsupported OSC argument type '" << arg->TypeTag() << "', using null" << std::endl;
            arguments.call<void>("push", emscripten::val::null());
        }
    }
}

/**
 * A helper function to parse a JS uInt8Array representing an
 * OSC message or bundle (such as returned by scsynth through its JS callback)
 * into a JS object.
 *
 * For bundles: address is "#bundle", arguments is an array of ParsedOscMessage objects.
 * For messages: address is the OSC address, arguments is an array of argument values.
 */
EMSCRIPTEN_KEEPALIVE
ParsedOscMessage parseOscBlobToJs(const emscripten::val& uint8Array) {
    unsigned int length = uint8Array["length"].as<unsigned int>();

    // first transfer the byte array into a vec<byte>
    std::vector<uint8_t> byteArray;
    byteArray.resize(length);
    auto memory = emscripten::val::module_property("HEAPU8")["buffer"];
    auto memoryView = uint8Array["constructor"].new_(memory, reinterpret_cast<uintptr_t>(byteArray.data()), length);
    memoryView.call<void>("set", uint8Array);
    // and then use its data pointer to pass it to world
    char* mBufPtr = reinterpret_cast<char*>(byteArray.data());

    const auto packet = osc::ReceivedPacket(mBufPtr, (size_t)length);

    ParsedOscMessage parsedMessage {};

    if (packet.IsBundle()) {
        parsedMessage.address = "#bundle";
        osc::ReceivedBundle bundle(packet);
        for (auto elem = bundle.ElementsBegin(); elem != bundle.ElementsEnd(); ++elem) {
            if (elem->IsMessage()) {
                osc::ReceivedMessage subMsg(*elem);
                ParsedOscMessage subParsed {};
                subParsed.address = subMsg.AddressPattern();
                parseOscArgs(subMsg, subParsed.arguments);
                // push as a JS object with address and arguments fields
                auto obj = emscripten::val::object();
                obj.set("address", emscripten::val(subParsed.address));
                obj.set("arguments", subParsed.arguments);
                parsedMessage.arguments.call<void>("push", obj);
            }
        }
    } else {
        osc::ReceivedMessage message(packet);
        parsedMessage.address = message.AddressPattern();
        parseOscArgs(message, parsedMessage.arguments);
    }

    return parsedMessage;
}

EMSCRIPTEN_BINDINGS(OSC_Helper) {
    emscripten::class_<OscMessageBuilder>("OscMessage")
        .constructor<>()
        .function("beginMessage", &OscMessageBuilder::beginMessage)
        .function("endMessage", &OscMessageBuilder::endMessage)
        .function("beginBundle", &OscMessageBuilder::beginBundle)
        .function("endBundle", &OscMessageBuilder::endBundle)
        .function("addBlob", &OscMessageBuilder::addBlob)
        .function("addInt", &OscMessageBuilder::addInt)
        .function("addFloat", &OscMessageBuilder::addFloat)
        .function("addString", &OscMessageBuilder::addString)
        .function("getData", &OscMessageBuilder::getData, emscripten::allow_raw_pointers());

    emscripten::value_object<ParsedOscMessage>("ParsedOscMessage")
        .field("address", &ParsedOscMessage::address)
        .field("arguments", &ParsedOscMessage::arguments);

    emscripten::function("parseOscMessage", &parseOscBlobToJs, emscripten::allow_raw_pointers());
}

// Make bindings visible to the linker b/c otherwise dead code elimination kicks in.
// still need to be added to exported_functions,
// but by doing this our whole binding will be picked up.
extern "C" EMSCRIPTEN_KEEPALIVE void webOscBindingAnchor() {}
