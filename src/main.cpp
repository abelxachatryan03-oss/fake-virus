#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <string>
#include <vector>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

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
        const wchar_t* txt = kMessages[std::rand() % kMsgCount];
        std::thread([txt]() {
            MessageBoxW(nullptr, txt, L"Ой, всё",
                        MB_OK | MB_ICONWARNING | MB_TOPMOST);
        }).detach();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    return 0;
}

// ─────────── поворот экрана ───────────
static bool RotateScreen(DWORD orientation) {
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    if (!EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm)) return false;

    // при 90°/270° меняем ширину и высоту местами
    if (orientation == DMDO_90 || orientation == DMDO_270) {
        DWORD tmp = dm.dmPelsWidth;
        dm.dmPelsWidth  = dm.dmPelsHeight;
        dm.dmPelsHeight = tmp;
    }
    dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYORIENTATION;
    dm.dmDisplayOrientation = orientation;

    LONG res = ChangeDisplaySettingsExW(nullptr, &dm, nullptr,
                                        CDS_UPDATEREGISTRY, nullptr);
    return res == DISP_CHANGE_SUCCESSFUL;
}

static DWORD WINAPI ScreenTwister(LPVOID) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 90°
    RotateScreen(DMDO_90);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 180° — вот это как на фото "перевёрнутый"
    RotateScreen(DMDO_180);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 270°
    RotateScreen(DMDO_270);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // назад в нормальное
    RotateScreen(DMDO_DEFAULT);
    return 0;
}

static DWORD WINAPI CmdSpammer(LPVOID) {
    std::this_thread::sleep_for(std::chrono::seconds(7));
    for (int i = 0; i < 40; ++i) {
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
        const double t = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t0).count();
        int x = start.x + (int)(ampX * std::sin(t * 1.1));
        int y = start.y + (int)(ampY * std::sin(t * 1.9));
        SetCursorPos(x, y);
        std::this_thread::sleep_for(std::chrono::milliseconds(12));
    }
    return 0;
}

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
            L"Если это сообщение появилось впервые — не ссы, всё под контролем.\n\n"
            L"Код ошибки: SANCHES_HAX_0xDEADPISKA\n"
            L"Виновник: Sanchez\n"
            L"Сообщение: Sanchez vzlomal tvoy pk piska pi piska\n\n"
            L"Что-то пошло не так, но не переживай — это шутка.\n"
            L"Через несколько секунд всё вернётся на круги своя.\n"
            L"— Jack & Fox";
        DrawTextW(dc, body, -1, &rt, DT_LEFT | DT_WORDBREAK);

        HFONT fSmall = CreateFontW(-20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        SelectObject(dc, fSmall);
        RECT rs{60, H - 80, W - 60, H - 20};
        DrawTextW(dc, L"Шутка. Реального вреда нет. Продолжай работать.",
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

static DWORD WINAPI FinalWord(LPVOID) {
    std::this_thread::sleep_for(std::chrono::seconds(16));
    // на всякий случай возвращаем экран в норму (если что-то пошло не так)
    RotateScreen(DMDO_DEFAULT);

    MessageBoxW(nullptr,
        L"Ладно, хватит.\n\nЭто была шутка. Ничего не удалено, ничего не украдено.\n"
        L"Если экран остался повёрнутым — верни через настройки дисплея.\n\n"
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

    CreateThread(nullptr, 0, MsgSpammer,     nullptr, 0, nullptr);
    CreateThread(nullptr, 0, CursorDancer,   nullptr, 0, nullptr);
    CreateThread(nullptr, 0, ScreenTwister,  nullptr, 0, nullptr);
    CreateThread(nullptr, 0, CmdSpammer,     nullptr, 0, nullptr);
    CreateThread(nullptr, 0, RedBSOD,        nullptr, 0, nullptr);
    CreateThread(nullptr, 0, FinalWord,      nullptr, 0, nullptr);

    Sleep(INFINITE);
    return 0;
}
