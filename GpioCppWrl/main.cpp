/*
Copyright(c) Microsoft Open Technologies, Inc. All rights reserved.

The MIT License(MIT)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

//
// GpioCppWrl
//
//   Utility to manipulate GPIO pins from the command line.
//   Demonstrates how to use the GPIO WinRT APIs from straight 
//   C++ with WRL.
//

#include <windows.h>
#include <strsafe.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <cwctype>

#include <windows.foundation.h>
#include <windows.devices.gpio.h>

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Devices::Gpio;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

class wexception
{
public:
    explicit wexception (const std::wstring &msg) : msg_(msg) { }
    virtual ~wexception () { }

    virtual const wchar_t *wwhat () const
    {
        return msg_.c_str();
    }

private:
    std::wstring msg_;
};

ComPtr<IGpioPin> MakePin (int PinNumber)
{
    ComPtr<IGpioPin> pin;
    
    // get the activation factory
    ComPtr<IGpioControllerStatics> controllerStatics;
    HRESULT hr = GetActivationFactory(
        HStringReference(RuntimeClass_Windows_Devices_Gpio_GpioController).Get(),
        &controllerStatics);
    if (FAILED(hr)) {
        std::wostringstream msg;
        msg << L"Failed to get activation factory for GpioController. hr = 0x" <<
            std::hex << hr;
        throw wexception(msg.str());
    }

    ComPtr<IGpioController> controller;
    hr = controllerStatics->GetDefault(controller.GetAddressOf());
    if (FAILED(hr)) {
        throw wexception(L"Failed to get instance of default GPIO controller");
    }

    if (!controller) {
        throw wexception(L"GPIO is not available on this system");
    }
    
    hr = controller->OpenPin(PinNumber, pin.GetAddressOf());
    if (FAILED(hr)) {
        std::wostringstream msg;
        msg << L"Failed to open pin. hr = 0x" << std::hex << hr;
        throw wexception(msg.str());
    }

    return pin;
}

GpioPinValue operator! (GpioPinValue Value)
{
    return (Value == GpioPinValue_Low) ? GpioPinValue_High : GpioPinValue_Low;
}

std::wistream& operator>> (std::wistream& is, GpioPinValue& value)
{
    int raw;
    is >> raw;
    if (is.fail()) {
        return is;
    }
    switch (raw) {
    case GpioPinValue_Low:
    case GpioPinValue_High:
        value = GpioPinValue(raw);
        break;
    default:
        is.clear(is.failbit);
    }
    return is;
}

std::wostream& operator<< (std::wostream& os, GpioPinValue value)
{
    switch (value) {
    case GpioPinValue_Low:
        return os << L"Low";
    case GpioPinValue_High:
        return os << L"High";
    default:
        return os << L"[undefined]";
    }
}

std::wistream& operator>> (std::wistream& is, GpioPinDriveMode& value)
{
    is.clear();
    std::wstring driveMode;
    is >> driveMode;
    if (!_wcsicmp(driveMode.c_str(), L"input")) {
        value = GpioPinDriveMode_Input;
    } else if (!_wcsicmp(driveMode.c_str(), L"output")) {
        value = GpioPinDriveMode_Output;
    } else if (!_wcsicmp(driveMode.c_str(), L"inputPullUp")) {
        value = GpioPinDriveMode_InputPullUp;
    } else if (!_wcsicmp(driveMode.c_str(), L"inputPullDown")) {
        value = GpioPinDriveMode_InputPullDown;
    } else {
        is.clear(is.failbit);
    }
    return is;
}

std::wostream& operator<< (std::wostream& os, GpioPinDriveMode value)
{
    switch (value) {
    case GpioPinDriveMode_Input:
        return os << L"input";
    case GpioPinDriveMode_Output:
        return os << L"output";
    case GpioPinDriveMode_InputPullUp:
        return os << L"inputPullUp";
    case GpioPinDriveMode_InputPullDown:
        return os << L"inputPullDown";
    default:
        return os << L"[undefined]";
    }
}

std::wostream& operator<< (std::wostream& os, GpioSharingMode value)
{
    switch (value) {
    case GpioSharingMode_Exclusive:
        return os << L"Exclusive";
    case GpioSharingMode_SharedReadOnly:
        return os << L"SharedReadOnly";
    default:
        return os << L"[undefined]";
    }
}

PCWSTR Help =
    L"Commands:\n"
    L" > write 0|1                        Write pin high or low\n"
    L" > toggle                           Toggle the pin from its current state\n"
    L" > read                             Read pin\n"
    L" > setdrivemode drive_mode          Set the pins's drive mode. Drive modes:\n"
    L"     where drive_mode = input|output|\n"
    L"                        inputPullUp|inputPullDown\n"
    L" > info                             Dump information about the pin\n"
    L" > help                             Display this help message\n"
    L" > quit                             Quit\n\n";

void ShowPrompt (_In_ IGpioPin* pin)
{
    GpioPinValue outputLatch = GpioPinValue_High;
    while (std::wcin) {
        std::wcout << L"> ";

        std::wstring line;
        if (!std::getline(std::wcin, line)) {
            return;
        }
        std::wistringstream linestream(line);

        std::wstring command;
        linestream >> command;
        if ((command == L"q") || (command == L"quit")) {
            return;
        } else if ((command == L"h") || (command == L"help")) {
            std::wcout << Help;
        } else if ((command == L"w") || (command == L"write")) {
            GpioPinValue value;
            linestream >> value;
            if (linestream.fail()) {
                std::wcout << L"Syntax error: expecting 0 or 1\n";
                std::wcout << L"Usage: write 0|1\n";
                continue;
            }

            HRESULT hr = pin->Write(value);
            if (FAILED(hr)) {
                std::wcout << L"Failed to write pin. hr = 0x" << std::hex << hr << "\n";
                continue;
            }
        } else if ((command == L"t") || (command == L"toggle")) {
            outputLatch = !outputLatch;
            HRESULT hr = pin->Write(outputLatch);
            if (FAILED(hr)) {
                std::wcout << L"Failed to write pin. hr = 0x" << std::hex << hr << "\n";
                continue;
            }
        } else if ((command == L"r") || (command == L"read")) {
            GpioPinValue value;
            HRESULT hr = pin->Read(&value);
            if (FAILED(hr)) {
                std::wcout << L"Failed to read pin. hr = 0x" << std::hex << hr << "\n";
                continue;
            }
            std::wcout << value << L"\n";
        } else if (command == L"setdrivemode") {
            GpioPinDriveMode driveMode;
            linestream >> driveMode;
            if (linestream.fail()) {
                std::wcout << L"Syntax error: expecting valid drive mode\n";
                continue;
            }
            
            HRESULT hr = pin->SetDriveMode(driveMode);
            if (FAILED(hr)) {
                std::wcout << L"Failed to set drive mode. hr = 0x" << std::hex << hr << "\n";
                continue;
            }
        } else if ((command == L"i") || (command == L"info")) {
            int pinNumber;
            pin->get_PinNumber(&pinNumber);
            GpioSharingMode sharingMode;
            pin->get_SharingMode(&sharingMode);
            TimeSpan debounceTimeout;
            pin->get_DebounceTimeout(&debounceTimeout);
            GpioPinDriveMode driveMode;
            pin->GetDriveMode(&driveMode);
            
            std::wcout << L"        Pin Number: " << std::dec << pinNumber << L"\n";
            std::wcout << L"      Sharing Mode: " << sharingMode << L"\n";
            std::wcout << L"  Debounce Timeout: " << debounceTimeout.Duration << L"\n";
            std::wcout << L"        Drive Mode: " << driveMode << L"\n";
        } else if (command.empty()) {
            // ignore
        } else {
            std::wcout << L"Unrecognized command: " << command <<
                L". Type 'help' for command usage.\n";
        }
    }
}

int __cdecl wmain (_In_ int argc, _In_reads_(argc) wchar_t *argv[])
{    
    wprintf(
        L"%s: Command line GPIO testing utility\n"
        L"  Usage: %s PinNumber\n"
        L"  Example: %s 47\n"
        L"  Type 'help' for a list of commands\n",
        argv[0],
        argv[0],
        argv[0]);

    if (argc < 2) {
        std::wcerr << L"Missing required command line parameter PinNumber\n";
        return 1;
    }

    wchar_t *endptr;
    int pinNumber = int(wcstoul(argv[1], &endptr, 0));
    std::wcout << L"  Pin Number: " << std::dec << pinNumber << L"\n";

    RoInitializeWrapper roInit(RO_INIT_MULTITHREADED);
    try {
        auto pin = MakePin(pinNumber);
        ShowPrompt(pin.Get());
    } catch (const wexception& ex) {
        std::wcerr << L"Fatal error occurred: " << ex.wwhat() << L"\n";
        return 1;
    }

    return 0;
}
