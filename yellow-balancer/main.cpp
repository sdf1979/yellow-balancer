#include <iostream>
#include <thread>
#include <time.h>
#include <io.h>
#include <fcntl.h>
#include "ProcessInfo.h"
#include "Logger.h"
#include "program_options.h"
#include "settings.h"

using time_point = std::chrono::time_point<std::chrono::system_clock>;

static auto LOGGER = Logger::getInstance();

static std::filesystem::path PROGRAM_PATH;
static std::filesystem::path FILE_PATH;
//static std::filesystem::path SETTINGS_PATH;

SERVICE_STATUS g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;

VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
VOID WINAPI ServiceCtrlHandler(DWORD);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);

wchar_t SERVICE_NAME[100] = L"Yellow Balancer Service";
static const std::wstring VERSION = L"1.0";

void RunConsole();
int InstallService(LPCWSTR serviceName, LPCWSTR servicePath);
int RemoveService(LPCWSTR serviceName);

BOOL SetPrivilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege) {
    TOKEN_PRIVILEGES tp;
    LUID luid;

    if (!LookupPrivilegeValue(
        NULL,            // lookup privilege on local system
        lpszPrivilege,   // privilege to lookup 
        &luid))        // receives LUID of privilege
    {
        printf("LookupPrivilegeValue error: %u\n", GetLastError());
        return FALSE;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    if (bEnablePrivilege)
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    else
        tp.Privileges[0].Attributes = 0;

    // Enable the privilege or disable all privileges.

    if (!AdjustTokenPrivileges(
        hToken,
        FALSE,
        &tp,
        sizeof(TOKEN_PRIVILEGES),
        (PTOKEN_PRIVILEGES)NULL,
        (PDWORD)NULL))
    {
        printf("AdjustTokenPrivileges error: %u\n", GetLastError());
        return FALSE;
    }

    DWORD err = GetLastError();
    if (err == ERROR_NOT_ALL_ASSIGNED) {
        std::string err_str = std::system_category().message(GetLastError());
        printf("The token does not have the specified privilege. \n");
        return FALSE;
    }

    return TRUE;
}

void GetPath() {
    WCHAR path[500];
    DWORD size = GetModuleFileNameW(NULL, path, 500);

    FILE_PATH = std::filesystem::path(path);
    PROGRAM_PATH = FILE_PATH.parent_path();
}

void SetLoggerLevel(Logger* logger, const std::wstring& level) {
    if (level == L"trace") logger->SetLogType(Logger::Trace);
    else if (level == L"info") logger->SetLogType(Logger::Info);
    else if (level == L"error") logger->SetLogType(Logger::Error);
}

int wmain(int argc, wchar_t** argv) {

    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    ProgrammOptions program_options(argc, argv);
    GetPath();
    LOGGER->Open(PROGRAM_PATH);
    SetLoggerLevel(LOGGER, L"info");

    if (program_options.IsHelp() || argc == 1) {
        std::wcout << program_options.Help();
        return 0;
    }

    /*
    HANDLE curProcess = GetCurrentProcess();
    HANDLE TokenHandle;
    if (OpenProcessToken(curProcess, TOKEN_ALL_ACCESS, &TokenHandle)) {
        SetPrivilege(TokenHandle, SE_DEBUG_NAME, TRUE);
        CloseHandle(TokenHandle);
    }
    */

    std::wstring mode = program_options.Mode();
    if (mode == L"console") {
        LOGGER->SetOutConsole(true);
        LOGGER->Print(L"Yellow Balancer: run console mode", true);
        LOGGER->Print(std::wstring(L"Version: ").append(VERSION), true);
        RunConsole();
        return 0;
    }
    else if (mode == L"install") {
        LOGGER->SetOutConsole(true);
        int result = InstallService(SERVICE_NAME, std::wstring(L"\"").append(FILE_PATH.wstring()).append(L"\" -M service").c_str());
        if (result == 0) {
            LOGGER->Print(std::wstring(L"Binary path: \"").append(FILE_PATH.wstring().append(L"\"")), true);
            Settings settings;
            settings.Read(PROGRAM_PATH);
        }
        return result;
    }
    else if (mode == L"uninstall") {
        LOGGER->SetOutConsole(true);
        return RemoveService(SERVICE_NAME);
    }
    else if (mode == L"service") {
        LOGGER->Print(L"Yellow Balancer: run service mode", true);
        LOGGER->Print(std::wstring(L"Version: ").append(VERSION), true);
    }
    else {
        return 0;
    }

    SERVICE_TABLE_ENTRY ServiceTable[] = {
        {SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        {NULL, NULL}
    };

    if (StartServiceCtrlDispatcher(ServiceTable) == FALSE) {
        auto error = GetLastError();
        if (error == 1063) {
            LOGGER->SetOutConsole(true);
            LOGGER->Print("Set startup mode = console. For more details, see the help.", Logger::Type::Error);
            return 0;
        }
        else {
            LOGGER->Print(std::wstring(L"Yellow Balancer: Main: StartServiceCtrlDispatcher returned error ").append(std::to_wstring(error)), Logger::Type::Error);
        }
        return error;
    }

    LOGGER->Print(L"Yellow Balancer: stop service", true);
}

void RunStep(std::shared_ptr<ProcessesInfo> p_processes_info) {
    p_processes_info->Read();
    p_processes_info->SetAffinity();
}

void RunConsole() {
    Settings settings;
    settings.Read(PROGRAM_PATH);
    ProcessesInfo processes_info;
    for (auto it = settings.Processes().begin(); it < settings.Processes().end(); ++it) {
        processes_info.AddFilter(*it);
    }
    processes_info.Read();
    processes_info.SetAffinity();
}

int InstallService(LPCWSTR serviceName, LPCWSTR servicePath) {
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCManager) {
        LOGGER->Print(L"Can't open Service Control Manager.", Logger::Type::Error, true);
        return -1;
    }

    SC_HANDLE hService = CreateService(
        hSCManager,
        serviceName,
        serviceName,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        servicePath,
        NULL, NULL, NULL, NULL, NULL
    );

    if (!hService) {
        int err = GetLastError();
        switch (err) {
        case ERROR_ACCESS_DENIED:
            LOGGER->Print(L"ERROR_ACCESS_DENIED", Logger::Type::Error, true);
            break;
        case ERROR_CIRCULAR_DEPENDENCY:
            LOGGER->Print(L"ERROR_CIRCULAR_DEPENDENCY", Logger::Type::Error, true);
            break;
        case ERROR_DUPLICATE_SERVICE_NAME:
            LOGGER->Print(L"ERROR_DUPLICATE_SERVICE_NAME", Logger::Type::Error, true);
            break;
        case ERROR_INVALID_HANDLE:
            LOGGER->Print(L"ERROR_INVALID_HANDLE", Logger::Type::Error, true);
            break;
        case ERROR_INVALID_NAME:
            LOGGER->Print(L"ERROR_INVALID_NAME", Logger::Type::Error, true);
            break;
        case ERROR_INVALID_PARAMETER:
            LOGGER->Print(L"ERROR_INVALID_PARAMETER", Logger::Type::Error, true);
            break;
        case ERROR_INVALID_SERVICE_ACCOUNT:
            LOGGER->Print(L"ERROR_INVALID_SERVICE_ACCOUNT", Logger::Type::Error, true);
            break;
        case ERROR_SERVICE_EXISTS:
            LOGGER->Print(L"ERROR_SERVICE_EXISTS", Logger::Type::Error, true);
            break;
        default:
            LOGGER->Print(L"Undefined", Logger::Type::Error, true);
        }
        CloseServiceHandle(hSCManager);
        return -1;
    }

    SERVICE_DESCRIPTION info;
    info.lpDescription = LPWSTR(L"Yellow Balancer Service: process balancer by numa groups");
    ChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &info);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    LOGGER->Print(L"Success install service!", true);

    return 0;
}

int RemoveService(LPCWSTR serviceName) {
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) {
        LOGGER->Print(L"Can't open Service Control Manager", Logger::Type::Error, true);
        return -1;
    }

    SC_HANDLE hService = OpenService(hSCManager, serviceName, SERVICE_STOP | DELETE);
    if (!hService) {
        LOGGER->Print(L"Can't remove service", Logger::Type::Error, true);
        CloseServiceHandle(hSCManager);
        return -1;
    }

    DeleteService(hService);
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    LOGGER->Print(L"Success remove service!", true);

    return 0;
}

DWORD WINAPI ServiceWorkerThread(LPVOID lpParam) {
    LOGGER->Print("Yellow Watcher: ServiceWorkerThread: Entry", Logger::Type::Trace);

    Settings settings;
    settings.Read(PROGRAM_PATH);
    int analysis_period = settings.AnalysisPeriod();

    std::shared_ptr<ProcessesInfo> p_processes_info = std::make_shared<ProcessesInfo>();
    for (auto it = settings.Processes().begin(); it < settings.Processes().end(); ++it) {
        p_processes_info->AddFilter(*it);
    }

    time_point last_run = {};
    time_point cur_run = std::chrono::system_clock::now();

    while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0) {
        std::int64_t period = std::chrono::duration_cast<std::chrono::seconds>(cur_run - last_run).count();
        if (period >= analysis_period) {
            RunStep(p_processes_info);
            last_run = cur_run;
        }
        cur_run = std::chrono::system_clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    LOGGER->Print(L"Yellow Balancer: ServiceWorkerThread: Exit", Logger::Type::Trace);

    return ERROR_SUCCESS;
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    DWORD Status = E_FAIL;

    LOGGER->Print("Yellow Balancer: ServiceMain: Entry");

    g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);
    if (g_StatusHandle == NULL) {
        LOGGER->Print(L"Yellow Balancer: ServiceMain: RegisterServiceCtrlHandler returned error", Logger::Type::Error);
        LOGGER->Print(L"Yellow Balancer: ServiceMain: Exit", Logger::Type::Info, true);
        return;
    }

    ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE) {
        LOGGER->Print(L"Yellow Balancer: ServiceMain: SetServiceStatus returned error", Logger::Type::Error);
    }

    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL) {
        LOGGER->Print(L"Yellow Balancer: ServiceMain: CreateEvent(g_ServiceStopEvent) returned error", Logger::Type::Error);

        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        g_ServiceStatus.dwCheckPoint = 1;

        if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE) {
            LOGGER->Print(L"Yellow Balancer: ServiceMain: SetServiceStatus returned error", Logger::Type::Error);
        }
        LOGGER->Print(L"Yellow Balancer: ServiceMain: Exit", Logger::Type::Info, true);
        return;
    }

    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE) {
        LOGGER->Print(L"Yellow Balancer: ServiceMain: SetServiceStatus returned error", Logger::Type::Error);
    }

    HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(g_ServiceStopEvent);

    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 3;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE) {
        LOGGER->Print(L"Yellow Balancer: ServiceMain: SetServiceStatus returned error", Logger::Type::Error);
    }

    LOGGER->Print(L"Yellow Balancer: ServiceMain: Exit", Logger::Type::Info, true);

    return;
}

VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode) {
    switch (CtrlCode) {
    case SERVICE_CONTROL_STOP:
        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING) {
            break;
        }
        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        g_ServiceStatus.dwWin32ExitCode = 0;
        g_ServiceStatus.dwCheckPoint = 4;

        if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE) {
            LOGGER->Print(L"Yellow Balancer: ServiceCtrlHandler: SetServiceStatus returned error", Logger::Type::Error);
        }

        SetEvent(g_ServiceStopEvent);
        break;
    default:
        break;
    }
}