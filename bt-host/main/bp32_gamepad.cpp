// SPDX-License-Identifier: Apache-2.0
#include "bp32_gamepad.h"
#include <Bluepad32.h>

static constexpr float AXIS_RANGE       = 512.0f; ///< Bluepad32 stick axes: -512..511.
static constexpr float THROTTLE_DEADBAND = 0.08f; ///< Dead band for throttle up/down classification (matches iPega's IPEGA_THROTTLE_DEADBAND).

static float clampf(float v) { return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v); }

static ControllerPtr s_controllers[BP32_MAX_GAMEPADS];

static void onConnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (s_controllers[i] == nullptr) {
            s_controllers[i] = ctl;
            Console.printf("[BP32] Connected: index=%d model=%s\n", i, ctl->getModelName());
            return;
        }
    }
}

static void onDisconnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (s_controllers[i] == ctl) {
            Console.printf("[BP32] Disconnected: index=%d\n", i);
            s_controllers[i] = nullptr;
            return;
        }
    }
}

void Bp32Gamepad::begin() {
    BP32.setup(&onConnectedController, &onDisconnectedController);
    BP32.forgetBluetoothKeys();
    BP32.enableVirtualDevice(false);
    BP32.enableBLEService(false);
}

void Bp32Gamepad::update() {
    BP32.update();

    ControllerPtr ctl = nullptr;
    for (auto c : s_controllers) {
        if (c && c->isConnected() && c->isGamepad()) { ctl = c; break; }
    }

    if (!ctl) {
        _status = Bp32Status::Scanning;
        _axes = GamepadAxes{};
        return;
    }
    _status = Bp32Status::Connected;

    // Matches the iPega convention (the only proven-working physical mapping
    // in this codebase): left stick -> yaw (X) + throttle (Y); right stick ->
    // roll (X) + pitch (Y). Bluepad32's axisX/axisY are the LEFT stick,
    // axisRX/axisRY are the RIGHT stick.
    _axes.connected = true;
    _axes.roll      = clampf((float)ctl->axisRX() / AXIS_RANGE);
    _axes.pitch     = clampf(-(float)ctl->axisRY() / AXIS_RANGE);
    _axes.yaw = clampf((float)ctl->axisX() / AXIS_RANGE);

    float ly = (float)ctl->axisY() / AXIS_RANGE;  // up = negative
    _axes.throttleUp   = (ly < -THROTTLE_DEADBAND) ? clampf(-ly) : 0.0f;
    _axes.throttleDown = (ly >  THROTTLE_DEADBAND) ? clampf(ly)  : 0.0f;

    uint16_t btns = 0;
    if (ctl->a()) btns |= GamepadBtn::A;
    if (ctl->b()) btns |= GamepadBtn::B;
    if (ctl->x()) btns |= GamepadBtn::X;
    if (ctl->y()) btns |= GamepadBtn::Y;
    if (ctl->dpad() & DPAD_UP)    btns |= GamepadBtn::DpadUp;
    if (ctl->dpad() & DPAD_DOWN)  btns |= GamepadBtn::DpadDown;
    if (ctl->dpad() & DPAD_LEFT)  btns |= GamepadBtn::DpadLeft;
    if (ctl->dpad() & DPAD_RIGHT) btns |= GamepadBtn::DpadRight;
    if (ctl->l1()) btns |= GamepadBtn::LT;
    if (ctl->r1()) btns |= GamepadBtn::R1;
    _axes.buttons = btns;
}
