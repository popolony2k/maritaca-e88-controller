#pragma once
#include <Arduino.h>
#include "accel_controller.h"
#include "gamepad_controller.h"
#include "operation_mode.h"
#include "../comm/drone_protocol.h"
#include "../imu/accelerometer.h"
#include "../bt/gamepad_axes.h"
#include "../hal/hal.h"

/**
 * @brief Flight phase states.
 */
enum class FlightState {
    Idle,        ///< Disarmed; sending keepalive packets.
    Calibrating, ///< Sending CaliGyro command for 1.5 s before arming.
    Arming,      ///< Sending Unlock then TakeOff command sequence.
    Flying,      ///< Active flight; control packets sent at 25 Hz.
    Landing,     ///< Sending Land command for 2 s, then returning to Idle.
    Emergency,   ///< Sending EmergStop once, then immediately returning to Idle.
};

/** @brief Return the display name of a FlightState (e.g. "FLYING"). */
const char* flightStateName(FlightState s);

/**
 * @brief External dependencies injected into FlightController at construction.
 */
struct FlightDeps {
    const ButtonHal& button; ///< Screen button abstraction.
    DroneProtocol&   drone;  ///< UDP control protocol driver.
};

/**
 * @brief Flight state machine and button gesture handler.
 *
 * Manages the full Idle → Calibrating → Arming → Flying → Landing lifecycle.
 * Button gestures drive state transitions:
 *   - Double-click in Idle  → begin calibration (requires WiFi).
 *   - Double-click in Flying → begin landing.
 *   - Triple-click (any state) → emergency stop.
 *   - Single-click in Flying → toggle yaw enable/disable.
 *
 * In AccelControl mode, a hold gesture adjusts throttle via AccelController.
 * In BluetoothControl mode, hold detection is disabled (ZL/ZR handle throttle).
 *
 * Safety: if WiFi is lost during any active flight state, an emergency stop
 * is triggered immediately and the machine returns to Idle.
 */
class FlightController {
public:
    /** @param deps  Button and drone dependencies (injected, not owned). */
    explicit FlightController(FlightDeps deps);

    /** @brief Reset state to Idle. Call once in setup(). */
    void begin();

    /** @brief Set the active control mode (AccelControl or BluetoothControl). */
    void setMode(OperationMode m) { _mode = m; }

    /**
     * @brief Advance the state machine one tick.
     *
     * Processes button gestures, runs state logic, and writes control values
     * to the drone via FlightDeps::drone.
     *
     * @param imu      Latest IMU sample (used in AccelControl mode).
     * @param gamepad  Latest gamepad axes (used in BluetoothControl mode).
     * @param wifiOk   True when the drone WiFi AP is connected.
     */
    void update(const ImuData& imu, const GamepadAxes& gamepad, bool wifiOk);

    /** @brief Current flight phase. */
    FlightState state() const { return _state; }

private:
    /**
     * @brief Transition to a new flight state.
     * @param s            Target state.
     * @param sendModeCmd  If true and transitioning to Idle, calls exitAppMode().
     */
    void enterState(FlightState s, bool sendModeCmd = true);

    /**
     * @brief Process button press/release events and detect gestures.
     * @param wifiOk  Passed to handleDoubleClick() to gate arming.
     */
    void handleButton(bool wifiOk);

    /**
     * @brief React to a confirmed double-click gesture.
     * @param wifiOk  If false and in Idle, the arm attempt is rejected.
     */
    void handleDoubleClick(bool wifiOk);

    /**
     * @brief Execute per-frame logic for the current state.
     * @param imu     IMU data forwarded to AccelController.
     * @param wifiOk  Triggers emergency stop if false during active flight.
     */
    void runState(const ImuData& imu, bool wifiOk);

    FlightDeps    _deps;
    OperationMode _mode            = OperationMode::AccelControl;
    FlightState   _state           = FlightState::Idle;
    uint32_t      _stateEnteredMs  = 0;
    bool          _stateFirstFrame = true;

    // ---- Button gesture state ------------------------------------------
    uint8_t  _clickCount    = 0;
    uint32_t _lastReleaseMs = 0;
    bool     _buttonDown    = false;
    bool     _btnIsHold     = false;
    bool     _btnHoldIsDown = false;
    bool     _yawEnabled    = false;

    GamepadAxes       _lastGamepadAxes;
    AccelController   _accel;
    GamepadController _gamepad;

    static constexpr uint32_t DOUBLE_CLICK_MS     =  300; ///< Max gap between clicks (ms).
    static constexpr uint32_t HOLD_THRESHOLD_MS   =  500; ///< Press duration to trigger hold.
    static constexpr uint32_t CALI_DURATION_MS    = 1500; ///< Gyro calibration duration.
    static constexpr uint32_t UNLOCK_DURATION_MS  =  500; ///< Unlock command duration.
    static constexpr uint32_t TAKEOFF_DURATION_MS =  500; ///< TakeOff command duration.
    static constexpr uint32_t ACCEL_LOCKOUT_MS    = 2000; ///< Post-takeoff input lockout.
    static constexpr uint32_t LANDING_DURATION_MS = 2000; ///< Landing command duration.
    static constexpr float    THROTTLE_HOLD_RATE  =  1.0f; ///< Throttle adjust rate during hold.
};
