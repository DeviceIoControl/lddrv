// lddrv.cpp : This file contains the 'main' function. Program execution begins and ends there.

#include <iostream>
#include <Windows.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <winternl.h>

#include  "..\SvcManager\SvcManager.hpp"

using T_NtLoadDriver = NTSTATUS(__stdcall*)(PUNICODE_STRING DriverServiceName);
using T_NtUnloadDriver = NTSTATUS(__stdcall*)(PUNICODE_STRING DriverServiceName);

class DriverManager
{
public:
    static bool Initialise() 
    {
        HMODULE hNTDLL = GetModuleHandleA("ntdll.dll");
        DriverManager::s_pfnNtLoadDriver = reinterpret_cast<T_NtLoadDriver>(GetProcAddress(hNTDLL, "NtLoadDriver"));
        DriverManager::s_pfnNtUnloadDriver = reinterpret_cast<T_NtUnloadDriver>(GetProcAddress(hNTDLL, "NtUnloadDriver"));

        std::cout << "Obtaining required privileges...\n";
        DriverManager::GetLoadDriverPrivilege();
    }

    static bool LoadDriver(const std::wstring& DriverServiceName) 
    {
        UNICODE_STRING wszDriverName = { 0 };
        wszDriverName.Buffer = const_cast<PWSTR>(DriverServiceName.data());
        wszDriverName.Length = DriverServiceName.length() * sizeof(wchar_t);
        wszDriverName.MaximumLength = wszDriverName.Length + sizeof(wchar_t);

        NTSTATUS status = DriverManager::s_pfnNtLoadDriver(&wszDriverName);
        return (status) ? false : true;
    }

    static bool UnloadDriver(const std::wstring& DriverServiceName)
    {
        UNICODE_STRING wszDriverName = { 0 };
        wszDriverName.Buffer = const_cast<PWSTR>(DriverServiceName.data());
        wszDriverName.Length = DriverServiceName.length() * sizeof(wchar_t);
        wszDriverName.MaximumLength = wszDriverName.Length + sizeof(wchar_t);
    
        NTSTATUS status = DriverManager::s_pfnNtUnloadDriver(&wszDriverName);
        return (status) ? false : true;
    }

    static bool Shutdown() 
    {
        DriverManager::s_pfnNtLoadDriver = nullptr;
        DriverManager::s_pfnNtUnloadDriver = nullptr;
        return true;
    }

private:
    inline static T_NtLoadDriver s_pfnNtLoadDriver;
    inline static T_NtUnloadDriver s_pfnNtUnloadDriver;

    static bool GetLoadDriverPrivilege() 
    {
        bool Success = true;
        HANDLE hProcessToken = NULL;
        OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hProcessToken);
        
        DWORD dwReturnLength = NULL;

        TOKEN_PRIVILEGES NewTknPrivs = { 0 };
        NewTknPrivs.PrivilegeCount = 1;
        NewTknPrivs.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        LookupPrivilegeValueA(nullptr, "SeLoadDriverPrivilege", &NewTknPrivs.Privileges[0].Luid);

        if (AdjustTokenPrivileges(hProcessToken, false, &NewTknPrivs, NULL, nullptr, &dwReturnLength))
        {
            TOKEN_PRIVILEGES* pOldTknPrivs = (decltype(pOldTknPrivs))::operator new(dwReturnLength);
            if (!AdjustTokenPrivileges(hProcessToken, false, &NewTknPrivs, dwReturnLength, pOldTknPrivs, &dwReturnLength))
            {
                std::cout << "Unable to gain SeLoadDriverPrivilege.\n";
                Success = false;
            }
        }

        CloseHandle(hProcessToken);
        return Success;
    }

};

int main(int argc, const char** argv)
{
    std::cout << "lddrv - Load Driver command-line utility\n";
    std::cout << "Created by Josh S.\n\n";

    std::cout << "Creating driver service...\n";
    
    ServiceManager::Initialize();
    DriverManager::Shutdown();

    if (argc < 5) 
    {
        std::cout << "Insufficient arguments provided.\n";
        ServiceManager::Shutdown();
        return ERROR_INVALID_PARAMETER;
    }

    std::unordered_map<std::string_view, std::string_view> ArgumentMap;
    ArgumentMap[std::string_view("-binpath")] = std::string_view();
    ArgumentMap[std::string_view("-svcname")] = std::string_view();
    ArgumentMap[std::string_view("-operation")] = std::string_view();

    for (int i = 1; i < argc; ++i) 
    {
        if (ArgumentMap.find(argv[i]) == ArgumentMap.end()) 
        {
            std::cout << "Invalid argument was provided: " << argv[i] << "\n";
            ServiceManager::Shutdown();
            return ERROR_INVALID_PARAMETER;
        }

        //Get the parameter for each argument provided and assign it to each of the arguments in Argument map.
        ArgumentMap[argv[i++]] = ArgumentMap[argv[i + 1]];
    }

    const std::string_view& svcName = ArgumentMap[std::string_view("-svcname")];
    const std::string_view& binPath = ArgumentMap[std::string_view("-binpath")];

    ServiceHandle hDrvService = ServiceManager::CreateService(svcName.data(), svcName.data(),
        SVC_TYPE::KERNEL_DRIVER, SVC_START_TYPE::MANUAL, SVC_ERROR_CTRL::ERROR_NORMAL, 
        binPath.data());

    HANDLE hProcessToken = NULL;
    std::cout << "Attempting to load driver...\n";
    
    //Convert from ASCII to wide-char string.
    std::wstring DriverName(svcName.begin(), svcName.end());
    DriverManager::LoadDriver(DriverName);



    
    DriverManager::UnloadDriver(DriverName);
    ServiceManager::DeleteService(hDrvService);
    
    ServiceManager::Shutdown();
    DriverManager::Shutdown();

    std::getchar();
    return ERROR_SUCCESS;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
