# Arduino USB Host Shield 2.0 HID Mouse Passthrough & Serial Controller

An embedded C++ firmware for Arduino Leonard / Pro Micro paired with a USB Host Shield 2.0. This firmware acts as a hardware USB HID proxy—intercepting physical mouse reports from a USB mouse connected to the host shield, forwarding button states, and allowing real-time relative cursor movement commands over a USB Serial (CDC/COM port) connection.

---

### Hardware Requirements
- **Arduino Board**: Arduino Leonardo, Pro Micro (ATmega32U4) or any board with native USB capability (`Mouse.h`).
- **USB Host Shield 2.0**: Standard MAX3421E-based USB Host Shield (or mini host shield).
- **USB Mouse**: Physical USB HID mouse connected to the USB Host Shield port.

### Libraries
- `Mouse.h` (Built-in Arduino USB HID library)
- [USB Host Shield Library 2.0](https://github.com/felis/USB_Host_Shield_2.0) (`usbhub.h`, `hidboot.h`)
- `SPI.h` & `Wire.h`

---

```
+------------------+         +----------------------------+         +------------------+
| Physical Mouse   | USB HID | Arduino + USB Host Shield  | USB CDC | Host PC / App    |
| (Connected to    | ------> |  - Intercepts HW inputs    | <-----> | (Serial COM Port |
| Host Shield Port)|         |  - Merges Serial offsets   |  115200 | @ 115200 baud)   |
+------------------+         |  - Emulates USB HID Mouse  |         +------------------+
                             +----------------------------+
                                           |
                                           v
                                 +--------------------+
                                 | PC OS Input Subsys |
                                 +--------------------+
```

1. **Hardware Passthrough**: The `MouseReportParser` captures movement vectors (`dX`, `dY`) and button states (LMB, RMB, MMB) directly from the physical mouse via the MAX3421E SPI host shield.
2. **Serial Overrides**: The main loop polls the USB CDC Serial interface (`Serial.available()`). When a 2-byte movement payload `[signed char dx, signed char dy]` is received over the COM port, the firmware moves the cursor by the specified relative offset.
3. **Default Passthrough**: If no serial data is queued, physical mouse movement inputs are passed through directly to maintaining seamless user control.

---

```cpp
#include <Mouse.h>
#include <Wire.h>
#include <SPI.h>
#include <usbhub.h>
#include <hidboot.h>

USB Usb;
USBHub Hub(&Usb);
HIDBoot<USB_HID_PROTOCOL_MOUSE> HidMouse(&Usb);

byte serialBuffer[2];

int dx = 0;
int dy = 0;
int lmb = 0;
int rmb = 0;
int mmb = 0;

class MouseRptParser : public MouseReportParser {
protected:
    void OnMouseMove(MOUSEINFO *mi) override;
    void OnLeftButtonUp(MOUSEINFO *mi) override;
    void OnLeftButtonDown(MOUSEINFO *mi) override;
    void OnRightButtonUp(MOUSEINFO *mi) override;
    void OnRightButtonDown(MOUSEINFO *mi) override;
    void OnMiddleButtonUp(MOUSEINFO *mi) override;
    void OnMiddleButtonDown(MOUSEINFO *mi) override;
};

void MouseRptParser::OnMouseMove(MOUSEINFO *mi) {
    dx = mi->dX;
    dy = mi->dY;
}

void MouseRptParser::OnLeftButtonUp(MOUSEINFO *mi) { lmb = 0; }
void MouseRptParser::OnLeftButtonDown(MOUSEINFO *mi) { lmb = 1; }
void MouseRptParser::OnRightButtonUp(MOUSEINFO *mi) { rmb = 0; }
void MouseRptParser::OnRightButtonDown(MOUSEINFO *mi) { rmb = 1; }
void MouseRptParser::OnMiddleButtonUp(MOUSEINFO *mi) { mmb = 0; }
void MouseRptParser::OnMiddleButtonDown(MOUSEINFO *mi) { mmb = 1; }

MouseRptParser Parser;

void setup() {
    delay(5000);
    Mouse.begin();
    Serial.begin(115200);
    Serial.setTimeout(1);

    if (Usb.Init() == -1) {
        // Host shield initialization check
    }
    
    HidMouse.SetReportParser(0, &Parser);
}

void loop() {
    dx = 0;
    dy = 0;

    Usb.Task();

    // Process physical button events
    if (lmb == 0) Mouse.release(MOUSE_LEFT);
    else if (lmb == 1) Mouse.press(MOUSE_LEFT);

    if (rmb == 0) Mouse.release(MOUSE_RIGHT);
    else if (rmb == 1) Mouse.press(MOUSE_RIGHT);

    if (mmb == 0) Mouse.release(MOUSE_MIDDLE);
    else if (mmb == 1) Mouse.press(MOUSE_MIDDLE);

    // Process Serial COM input vector if present, otherwise pass physical delta
    if (Serial.available() > 0) {
        Serial.readBytes(serialBuffer, 2);
        Mouse.move((int8_t)serialBuffer[0], (int8_t)serialBuffer[1], 0);
    } else {
        Mouse.move(dx, dy);
    }
}
```

---

An example C++ Windows Win32 host controller application demonstrating how to open the Arduino CDC COM port and transmit 2-byte relative movement deltas `[int8_t dx, int8_t dy]`:

```cpp
#include <windows.h>
#include <iostream>
#include <cstdint>

class SerialMouseController {
private:
    HANDLE hSerial;

public:
    SerialMouseController(const wchar_t* portName) {
        hSerial = CreateFileW(
            portName,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hSerial == INVALID_HANDLE_VALUE) {
            std::cerr << "Failed to open serial port." << std::endl;
            return;
        }

        DCB dcbSerialParams = { 0 };
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

        if (!GetCommState(hSerial, &dcbSerialParams)) {
            std::cerr << "Error getting serial state." << std::endl;
            return;
        }

        dcbSerialParams.BaudRate = CBR_115200;
        dcbSerialParams.ByteSize = 8;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity   = NOPARITY;

        SetCommState(hSerial, &dcbSerialParams);

        COMMTIMEOUTS timeouts = { 0 };
        timeouts.ReadIntervalTimeout         = 1;
        timeouts.ReadTotalTimeoutConstant    = 1;
        timeouts.ReadTotalTimeoutMultiplier  = 1;
        timeouts.WriteTotalTimeoutConstant   = 1;
        timeouts.WriteTotalTimeoutMultiplier = 1;
        SetCommState(hSerial, &dcbSerialParams);
    }

    ~SerialMouseController() {
        if (hSerial != INVALID_HANDLE_VALUE) {
            CloseHandle(hSerial);
        }
    }

    bool SendMove(int8_t deltaX, int8_t deltaY) {
        if (hSerial == INVALID_HANDLE_VALUE) return false;

        uint8_t buffer[2] = { static_cast<uint8_t>(deltaX), static_cast<uint8_t>(deltaY) };
        DWORD bytesWritten = 0;

        return WriteFile(hSerial, buffer, 2, &bytesWritten, NULL) && bytesWritten == 2;
    }
};

int main() {
    // Replace COM3 with your Arduino Leonardo/Pro Micro COM port
    SerialMouseController mouseController(L"\\\\.\\COM3");

    // Move cursor right by 10 units and down by 5 units
    mouseController.SendMove(10, 5);

    return 0;
}
```

---

## 📜 License

Distributed under the MIT License. See `LICENSE` for details.
