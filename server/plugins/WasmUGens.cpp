/*
    SuperCollider real time audio synthesis system
    Copyright (c) 2002 James McCartney. All rights reserved.
    http://www.audiosynth.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include <iostream>
#include <SC_Lock.h>

#include "emscripten/html5.h"
#include "SC_PlugIn.h"
#include "../../include/server/SC_WorldOptions.h"

static InterfaceTable* ft;

struct KeyboardUGenGlobalState {
    uint8 keys[255];
} gKeyStateGlobals;

struct KeyState : public Unit {
    float m_y1, m_b1, m_lag;
};


struct MouseUGenGlobalState {
    int screenWidth, screenHeight;
    float mouseX, mouseY;
    bool mouseButton;
} gMouseUGenGlobals;

struct MouseInputUGen : public Unit {
    float m_y1, m_b1, m_lag;
};


//////////////////////////////////////////////////////////////////////////////////////////////////

void KeyState_next(KeyState* unit, int inNumSamples) {
    // minval, maxval, warp, lag
    uint8* keys = (uint8*)gKeyStateGlobals.keys;
    int keynum = (int)ZIN0(0);
    int val = keys[keynum];

    float minval = ZIN0(1);
    float maxval = ZIN0(2);
    float lag = ZIN0(3);

    float y1 = unit->m_y1;
    float b1 = unit->m_b1;

    if (lag != unit->m_lag) {
        unit->m_b1 = lag == 0.f ? 0.f : exp(log001 / (lag * unit->mRate->mSampleRate));
        unit->m_lag = lag;
    }
    float y0 = val ? maxval : minval;
    ZOUT0(0) = y1 = y0 + b1 * (y1 - y0);
    unit->m_y1 = zapgremlins(y1);
}

void KeyState_Ctor(KeyState* unit) {
    SETCALC(KeyState_next);
    unit->m_b1 = 0.f;
    unit->m_lag = 0.f;
    KeyState_next(unit, 1);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void MouseX_next(MouseInputUGen* unit, int inNumSamples) {
    // minval, maxval, warp, lag

    float minval = ZIN0(0);
    float maxval = ZIN0(1);
    float warp = ZIN0(2);
    float lag = ZIN0(3);

    float y1 = unit->m_y1;
    float b1 = unit->m_b1;

    if (lag != unit->m_lag) {
        unit->m_b1 = lag == 0.f ? 0.f : (float)exp(log001 / (lag * unit->mRate->mSampleRate));
        unit->m_lag = lag;
    }
    float y0 = gMouseUGenGlobals.mouseX;
    if (warp == 0.0) {
        y0 = (maxval - minval) * y0 + minval;
    } else {
        y0 = pow(maxval / minval, y0) * minval;
    }
    ZOUT0(0) = y1 = y0 + b1 * (y1 - y0);
    unit->m_y1 = zapgremlins(y1);
}

void MouseX_Ctor(MouseInputUGen* unit) {
    SETCALC(MouseX_next);
    unit->m_b1 = 0.f;
    unit->m_lag = 0.f;
    MouseX_next(unit, 1);
}


void MouseY_next(MouseInputUGen* unit, int inNumSamples) {
    // minval, maxval, warp, lag

    float minval = ZIN0(0);
    float maxval = ZIN0(1);
    float warp = ZIN0(2);
    float lag = ZIN0(3);

    float y1 = unit->m_y1;
    float b1 = unit->m_b1;

    if (lag != unit->m_lag) {
        unit->m_b1 = lag == 0.f ? 0.f : (float)exp(log001 / (lag * unit->mRate->mSampleRate));
        unit->m_lag = lag;
    }
    float y0 = gMouseUGenGlobals.mouseY;
    if (warp == 0.0) {
        y0 = (maxval - minval) * y0 + minval;
    } else {
        y0 = pow(maxval / minval, y0) * minval;
    }
    ZOUT0(0) = y1 = y0 + b1 * (y1 - y0);
    unit->m_y1 = zapgremlins(y1);
}

void MouseY_Ctor(MouseInputUGen* unit) {
    SETCALC(MouseY_next);
    unit->m_b1 = 0.f;
    unit->m_lag = 0.f;
    MouseY_next(unit, 1);
}


void MouseButton_next(MouseInputUGen* unit, int inNumSamples) {
    // minval, maxval, warp, lag

    float minval = ZIN0(0);
    float maxval = ZIN0(1);
    float lag = ZIN0(2);

    float y1 = unit->m_y1;
    float b1 = unit->m_b1;

    if (lag != unit->m_lag) {
        unit->m_b1 = lag == 0.f ? 0.f : (float)exp(log001 / (lag * unit->mRate->mSampleRate));
        unit->m_lag = lag;
    }
    float y0 = gMouseUGenGlobals.mouseButton ? maxval : minval;
    ZOUT0(0) = y1 = y0 + b1 * (y1 - y0);
    unit->m_y1 = zapgremlins(y1);
}

void MouseButton_Ctor(MouseInputUGen* unit) {
    SETCALC(MouseButton_next);
    unit->m_b1 = 0.f;
    unit->m_lag = 0.f;
    MouseButton_next(unit, 1);
}

static bool processMouseData(int eventType, const EmscriptenMouseEvent* mouseEvent, void* userData) {
    float width = static_cast<float>(gMouseUGenGlobals.screenWidth);
    float height = static_cast<float>(gMouseUGenGlobals.screenHeight);
    if (width > 0 && height > 0) {
        gMouseUGenGlobals.mouseX = mouseEvent->clientX / width;
        gMouseUGenGlobals.mouseY = mouseEvent->clientY / height;
    }
    gMouseUGenGlobals.mouseButton = mouseEvent->buttons > 0;
    // false = propagate event further
    return false;
}

static bool processScreenSize(int eventType, const EmscriptenUiEvent* uiEvent, void* userData) {
    gMouseUGenGlobals.screenWidth = uiEvent->windowInnerWidth;
    gMouseUGenGlobals.screenHeight = uiEvent->windowInnerHeight;
    return false;
}


static bool processKeydown(int eventType, const EmscriptenKeyboardEvent* keyEvent, void* userData) {
    auto key = static_cast<int>(keyEvent->key[0]);
    gKeyStateGlobals.keys[key] = 1;
    return false;
}

static bool processKeyup(int eventType, const EmscriptenKeyboardEvent* keyEvent, void* userData) {
    auto key = static_cast<int>(keyEvent->key[0]);
    gKeyStateGlobals.keys[key] = 0;
    return false;
}


PluginLoad(WasmUGens) {
    ft = inTable;

    // initialize screen dimensions
    gMouseUGenGlobals.screenWidth = EM_ASM_INT({ return window.innerWidth; });
    gMouseUGenGlobals.screenHeight = EM_ASM_INT({ return window.innerHeight; });

    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, processScreenSize);

    emscripten_set_mousemove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, processMouseData);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, processKeydown);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, processKeyup);

    DefineSimpleUnit(KeyState);

    DefineUnit("MouseX", sizeof(MouseInputUGen), (UnitCtorFunc)&MouseX_Ctor, 0, 0);
    DefineUnit("MouseY", sizeof(MouseInputUGen), (UnitCtorFunc)&MouseY_Ctor, 0, 0);
    DefineUnit("MouseButton", sizeof(MouseInputUGen), (UnitCtorFunc)&MouseButton_Ctor, 0, 0);
}


PluginUnload(WasmUGens) {}
