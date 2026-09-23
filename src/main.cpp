#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

static const wchar_t* kMessages[] = {
    L"даров Вась ты чё там ты там в порядке вась?",
    L"бро глаголит имбу😎🥲🥲🙏🥶🙏🐉🙏🐉🙏🥶🥀",
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
    for (int i = 0; i < 25; ++i) {
        const wchar_t* txt = kMessages[std::rand() % kMsgCount];
        std::thread([txt]() {
            MessageBoxW(nullptr, txt, L"Ой, всё",
                        MB_OK | MB_ICONWARNING | MB_TOPMOST);
        }).detach();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return 0;
}

static DWORD WINAPI CursorDancer(LPVOID) {
    POINT start{};
    GetCursorPos(&start);
    const double ampX = 250.0, ampY = 180.0;
    const auto t0 = std::chrono::steady_clock::now();

    while (std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now() - t0).count() < 8) {
        const double t = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t0).count();
        int x = start.x + (int)(ampX * std::sin(t * 1.1));
        int y = start.y + (int)(ampY * std::sin(t * 1.9));
        SetCursorPos(x, y);
        std::this_thread::sleep_for(std::chrono::milliseconds(12));
    }
    return 0;
}

static DWORD WINAPI FinalWord(LPVOID) {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    MessageBoxW(nullptr,
        L"Ладно, хватит.\n\nЭто была шутка. Ничего не удалено, ничего не украдено.\n"
        L"Скажи спасибо, что Fox добрый.\n\n— Jack & Fox",
        L"Всё, я ушёл",
        MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
    ExitProcess(0);
    return 0;
}

int main() {
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\JackFoxJokeVirus");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    if (HWND c = GetConsoleWindow()) ShowWindow(c, SW_HIDE);

    CreateThread(nullptr, 0, MsgSpammer,   nullptr, 0, nullptr);
    CreateThread(nullptr, 0, CursorDancer, nullptr, 0, nullptr);
    CreateThread(nullptr, 0, FinalWord,    nullptr, 0, nullptr);

    Sleep(INFINITE);
    return 0;
}    return 0;
}

static DWORD WINAPI CursorDancer(LPVOID) {
    POINT start{};
    GetCursorPos(&start);
    const double ampX = 250.0, ampY = 180.0;
    const auto t0 = std::chrono::steady_clock::now();

    while (std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now() - t0).count() < 8) {
        const double t = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t0).count();
        int x = start.x + (int)(ampX * std::sin(t * 1.1));
        int y = start.y + (int)(ampY * std::sin(t * 1.9));
        SetCursorPos(x, y);
        std::this_thread::sleep_for(std::chrono::milliseconds(12));
    }
    return 0;
}

static DWORD WINAPI FinalWord(LPVOID) {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    MessageBoxW(nullptr,
        L"Ладно, хватит.\n\nЭто была шутка. Ничего не удалено, ничего не украдено.\n"
        L"Скажи спасибо, что Fox добрый.\n\n— Jack & Fox",
        L"Всё, я ушёл",
        MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
    ExitProcess(0);
    return 0;
}

int main() {
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\JackFoxJokeVirus");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    if (HWND c = GetConsoleWindow()) ShowWindow(c, SW_HIDE);

    CreateThread(nullptr, 0, MsgSpammer,   nullptr, 0, nullptr);
    CreateThread(nullptr, 0, CursorDancer, nullptr, 0, nullptr);
    CreateThread(nullptr, 0, FinalWord,    nullptr, 0, nullptr);

    Sleep(INFINITE);
    return 0;
}
