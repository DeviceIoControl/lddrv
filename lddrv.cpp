// lddrv.cpp : This file contains the 'main' function. Program execution begins and ends there.

#include <iostream>
#include <Windows.h>
#include <string>
#include <string_view>
#include <memory>
#include <unordered_map>
#include <winternl.h>
#include <processthreadsapi.h>

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
        bool Success = DriverManager::GetLoadDriverPrivilege();

        if(Success)
        {
            std::cout << "Successfully obtained the privileges!\n";
        }
        else 
        {
            std::cout << "Unable to gain required privileges...\n";
        }

        return Success;
    }

    static bool LoadDriver(const std::wstring& DriverServiceName) 
    {
        UNICODE_STRING wszDriverName = { 0 };

        std::wstring FullDriverSvcPath(L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\");
        FullDriverSvcPath += DriverServiceName;

        wszDriverName.Buffer = const_cast<PWSTR>(FullDriverSvcPath.data());
        wszDriverName.Length = FullDriverSvcPath.length() * sizeof(wchar_t);
        wszDriverName.MaximumLength = wszDriverName.Length + sizeof(wchar_t);

        HRESULT status = DriverManager::s_pfnNtLoadDriver(&wszDriverName);
         
        return (status) ? false : true;
    }

    static bool UnloadDriver(const std::wstring& DriverServiceName)
    {
        UNICODE_STRING wszDriverName = { 0 };

        std::wstring FullDriverSvcPath(L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\");
        FullDriverSvcPath += DriverServiceName;

        wszDriverName.Buffer = const_cast<PWSTR>(FullDriverSvcPath.data());
        wszDriverName.Length = FullDriverSvcPath.length() * sizeof(wchar_t);
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

    static HANDLE GetProcessToken() 
    {
        bool Success = true;
        HANDLE hProcessToken = NULL;

        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hProcessToken)) 
        {
            std::cout << "Cannot open process token!\n";
            return INVALID_HANDLE_VALUE;
        }

        return hProcessToken;
    }

    static TOKEN_PRIVILEGES GetTokenPrivilegeFromName(const std::string& szPrivName)
    {
        TOKEN_PRIVILEGES NewTknPrivs = { 0 };
        NewTknPrivs.PrivilegeCount = 1;
        NewTknPrivs.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        if (!LookupPrivilegeValueA(nullptr, szPrivName.c_str(), &NewTknPrivs.Privileges[0].Luid))
        {
            std::cout << "Unable to gain SeLoadDriverPrivilege.\n";
        }

        return NewTknPrivs;
    }

    static bool ApplyTokenPrivilege(HANDLE hProcessToken, const TOKEN_PRIVILEGES& tokenPriv)
    {
        DWORD dwReturnLength = NULL;
        TOKEN_PRIVILEGES NewTokenPrivs = tokenPriv;
       
        if(!AdjustTokenPrivileges(hProcessToken, false, &NewTokenPrivs, NULL, nullptr, &dwReturnLength))
        {
            std::cout << "Unable to apply privileges to the process access token.\n";
            return false;
        }
        
        return true;
    }

    static bool GetLoadDriverPrivilege() 
    {
        HANDLE hProcessToken = DriverManager::GetProcessToken();

        if (hProcessToken == INVALID_HANDLE_VALUE) 
        {
            std::cout << "Unable to obtain process token...\n";
            return false;
        }

        TOKEN_PRIVILEGES tokenPriv = DriverManager::GetTokenPrivilegeFromName("SeLoadDriverPrivilege");

        bool Success = false;

        if (tokenPriv.PrivilegeCount != 1)
        {
            std::cout << "Unable to obtain TOKEN_PRIVILEGES...\n";    
        }
        else
        {
            Success = DriverManager::ApplyTokenPrivilege(hProcessToken, tokenPriv);
        }
        
        CloseHandle(hProcessToken);
        return Success;
    }
};

int main(int argc, const char** argv)
{
    std::cout << "lddrv - Load Driver command-line utility [Version 0.9.9a]\n";
    std::cout << "(c) Created by Josh S. All rights reserved.\n\n";

    if (argc < 5) 
    {
        std::cout << "Insufficient arguments provided.\n";
        return ERROR_INVALID_PARAMETER;
    }

    std::cout << "Initialising ServiceManager...\n";

    if (ServiceManager::Initialise()) 
    {
        std::cout << "Service Manager succesfully initialised!\n";
    }
    else 
    {
        std::cout << "Unable to intialise ServiceManager...\n";
        return E_FAIL;
    }

    std::cout << "Initialising DriverManager...\n";
    
    if (DriverManager::Initialise()) 
    {
        std::cout << "DriverManager successfully initialised!\n";
    }
    else 
    {
        std::cout << "Unable to intitalize DriverManager.\n";
        ServiceManager::Shutdown();
        return E_FAIL;
    }

    std::unordered_map<std::string_view, const char*> ArgumentMap;
    ArgumentMap.emplace("-binpath", nullptr);
    ArgumentMap.emplace("-svcname", nullptr);
    ArgumentMap.emplace("-operation", nullptr);

    // Get the parameter for each argument provided and assign it to it's corresponding argument in the Argument map.
    for (int i = 1; i < argc; ++i) 
    {
        auto argumentItrPos = ArgumentMap.find(argv[i]);

        // Ensure the argument is present in the arg map, and verify the parameter to an arg is not an arg. 
        // (E.g "-binpath -operation create" will not work.)
        if (argumentItrPos == ArgumentMap.end() || argv[i + 1][0] == '-')
        {
            std::cout << "Invalid argument was provided: " << argv[i] << "\n";
            
            DriverManager::Shutdown();
            ServiceManager::Shutdown();
            
            return ERROR_INVALID_PARAMETER;
        }

        argumentItrPos->second = argv[++i];
    }

    const std::string_view svcName = ArgumentMap[std::string_view("-svcname")];
    const std::string_view binPath = ArgumentMap[std::string_view("-binpath")];
    const std::string_view operation = ArgumentMap[std::string_view("-operation")];

    ServiceHandle hDriverService;

    //Convert from narrow to wide-char string.
    std::wstring DriverSvcName(svcName.begin(), svcName.end());
    
    if (operation == "create") 
    {
        std::cout << "Creating driver service...\n";
        hDriverService = ServiceManager::CreateService(svcName.data(), "Driver Display Name",
            SVC_TYPE::KERNEL_DRIVER, SVC_START_TYPE::MANUAL, SVC_ERROR_CTRL::ERROR_NORMAL,
            binPath.data());

        if (hDriverService.Valid()) 
        {
            std::cout << "Driver service was created succesfully!\n";
        }
        else 
        {
            std::cout << "Unable to create driver service...\n";

            DriverManager::Shutdown();
            ServiceManager::Shutdown();

            return E_FAIL;
        }

        std::cout << "Attempting to load driver...\n";    

        if (DriverManager::LoadDriver(DriverSvcName)) 
        {
            std::cout << "Driver was loaded successfully!\n";
        }
        else 
        {
            std::cout << "Failed to load driver.\n";
        }
    }
    else if (operation == "delete") 
    {
        hDriverService = ServiceManager::OpenService(svcName.data(), SVC_ACCESS::ALL_ACCESS);
        std::unique_ptr<QUERY_SERVICE_CONFIGA> svcConfig = hDriverService.QueryConfig();

        if (!svcConfig)
        {
            std::cout << "Unable to retrieve critical service information...\n";
            
            ServiceManager::Shutdown();
            DriverManager::Shutdown();

            return E_FAIL;
        }

        if (svcConfig->dwServiceType != (DWORD)SVC_TYPE::KERNEL_DRIVER || !hDriverService.Valid())
        {
            std::cout << "Unable to remove the driver service.\n";
        }
        else
        {
            std::cout << "Unloading driver...\n";
            DriverManager::UnloadDriver(DriverSvcName);
            
            std::cout << "Removing driver service...\n";
            ServiceManager::DeleteService(hDriverService);
        }
    }
 
    ServiceManager::Shutdown();
    DriverManager::Shutdown();

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
