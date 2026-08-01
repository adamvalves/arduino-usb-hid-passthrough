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
            std::cerr << "[!] Failed to open serial COM port." << std::endl;
            return;
        }

        DCB dcbSerialParams = { 0 };
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

        if (!GetCommState(hSerial, &dcbSerialParams)) {
            std::cerr << "[!] Failed to retrieve serial state." << std::endl;
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
        SetCommTimeouts(hSerial, &timeouts);
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
    std::cout << "[*] Initializing USB CDC Serial Connection to Arduino..." << std::endl;

    // Adjust port name to match your assigned Arduino COM port (e.g. \\.\COM3)
    SerialMouseController controller(L"\\\\.\\COM3");

    std::cout << "[*] Sending test move command (+10, +5)..." << std::endl;
    if (controller.SendMove(10, 5)) {
        std::cout << "[+] Movement payload successfully sent to device." << std::endl;
    } else {
        std::cerr << "[!] Failed to transmit move command." << std::endl;
    }

    return 0;
}
