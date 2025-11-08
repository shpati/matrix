#include <windows.h>
#include <stdlib.h>
#include <time.h>

#define MAX_TRAIL 64
#define MAX_COLS 1024

// ======= CONFIGURABLE PARAMETERS =======
#define FALL_SPEED_MIN   4
#define FALL_SPEED_MAX   10
#define TRAIL_LEN_MIN    15
#define TRAIL_LEN_MAX    25
#define LETTER_CYCLE_MIN 5
#define LETTER_CYCLE_MAX 10
#define BURST_CHANCE     50
#define FRAME_INTERVAL   20   // ms per frame 
// ======================================

typedef struct {
    float x, y;
    float speed;
    int trail;
    int refreshDelay;
    int refreshCounter;
    int burst;
    WCHAR chars[MAX_TRAIL];
} Drop;

static Drop drops[MAX_COLS];
static int ncols, width, height;
static int colW = 22, colH = 22;
static HFONT hFont;

static const WCHAR charset[] =
    L"0123456789"
    L"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    L"abcdefghijklmnopqrstuvwxyz"
    L"!@#$%^&*()-_=+[]{};:,.<>/?";

static inline int rnd(int a, int b) { return a + rand() % (b - a + 1); }

void init_drops(HWND hwnd) {
    RECT r; GetClientRect(hwnd, &r);
    width = r.right; height = r.bottom;

    ncols = (width + colW - 1) / colW;
    if (ncols > MAX_COLS) ncols = MAX_COLS;
    if (ncols < 1) ncols = 1;

    for (int i = 0; i < ncols; i++) {
        drops[i].x = (float)i * colW;
        drops[i].y = (float)rnd(-height * 2, height * 2);
        drops[i].speed = (float)rnd(FALL_SPEED_MIN, FALL_SPEED_MAX);
        drops[i].trail = rnd(TRAIL_LEN_MIN, TRAIL_LEN_MAX);
        // Tie letter cycling to fall speed
        float t = (drops[i].speed - FALL_SPEED_MIN) / (float)(FALL_SPEED_MAX - FALL_SPEED_MIN);
        drops[i].refreshDelay = (int)(LETTER_CYCLE_MAX - t * (LETTER_CYCLE_MAX - LETTER_CYCLE_MIN));
        drops[i].refreshCounter = rnd(0, drops[i].refreshDelay);
        drops[i].burst = (rand() % BURST_CHANCE == 0);

        for (int j = 0; j < MAX_TRAIL; j++)
            drops[i].chars[j] = charset[rand() %
                (sizeof(charset) / sizeof(WCHAR) - 1)];
    }
}

void draw_frame(HDC memdc) {
    // Clear backbuffer (single fill; very fast)
    RECT r = { 0, 0, width, height };
    FillRect(memdc, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));

    SetBkMode(memdc, TRANSPARENT);

    for (int i = 0; i < ncols; i++) {
        Drop *d = &drops[i];

        // Update letter cycling (only occasionally)
        if (--d->refreshCounter <= 0) {
            int visible = d->trail < MAX_TRAIL ? d->trail : MAX_TRAIL;
            for (int j = 0; j < visible; j++)
                d->chars[j] = charset[rand() % (sizeof(charset)/sizeof(WCHAR) - 1)];
            d->refreshCounter = d->refreshDelay;
        }

        // Draw trail from head (0) downward
        for (int j = 0; j < d->trail; j++) {
            int yy = (int)(d->y - j * colH);
            if (yy < -colH || yy >= height) continue;

            int brightness = (255 * (d->trail - j)) / d->trail;
            COLORREF col = d->burst
                ? RGB(brightness, brightness, brightness)
                : RGB(0, brightness / 2, 0);

            if (j == 0)
                col = d->burst ? RGB(255,255,255) : RGB(180,255,180);

            SetTextColor(memdc, col);
            TextOutW(memdc, (int)d->x, yy, &d->chars[j], 1);
        }

        // Move column down
        d->y += d->speed;

        // Reset only when *entire* trail is off-screen
        if (d->y - d->trail * colH > height) {
            d->y = (float)rnd(-height * 0.25f, 0);
            d->trail = rnd(TRAIL_LEN_MIN, TRAIL_LEN_MAX);
            d->burst = (rand() % BURST_CHANCE == 0);
        }
    }
}


LRESULT CALLBACK WndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    static HDC hdc = NULL, memdc = NULL;
    static HBITMAP bmp = NULL;
    static HGDIOBJ oldbmp = NULL;
    static HBRUSH bg = NULL;
    static int lastMouseX = -1;
    static int lastMouseY = -1;

    switch (msg) {
    case WM_CREATE: {
        ShowCursor(FALSE);
        hdc = GetDC(w);
        RECT r; GetClientRect(w, &r);
        width = r.right; height = r.bottom;

        hFont = CreateFontW(
            colH, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH, L"Consolas"
        );

        bmp = CreateCompatibleBitmap(hdc, width, height);
        memdc = CreateCompatibleDC(hdc);
        oldbmp = SelectObject(memdc, bmp);
        bg = CreateSolidBrush(RGB(0, 0, 0));

        SelectObject(memdc, hFont);
        SIZE s;
        GetTextExtentPoint32W(memdc, L"W", 1, &s);
        if (s.cx > 4 && s.cy > 4) { colW = s.cx; colH = s.cy; }

        init_drops(w);
        SetTimer(w, 1, FRAME_INTERVAL, NULL);
        return 0;
    }

    case WM_TIMER:
        draw_frame(memdc);
        BitBlt(hdc, 0, 0, width, height, memdc, 0, 0, SRCCOPY);
        return 0;

    case WM_DESTROY:
        ShowCursor(TRUE);
        if (memdc) { SelectObject(memdc, oldbmp); DeleteDC(memdc); }
        if (bmp) DeleteObject(bmp);
        if (bg) DeleteObject(bg);
        if (hFont) DeleteObject(hFont);
        if (hdc) ReleaseDC(w, hdc);
        PostQuitMessage(0);
        return 0;

    case WM_MOUSEMOVE: {
        int x = LOWORD(lp);
        int y = HIWORD(lp);

        if (lastMouseX == -1 && lastMouseY == -1) {
            lastMouseX = x;
            lastMouseY = y;
        } else if (abs(x - lastMouseX) > 3 || abs(y - lastMouseY) > 3) {
            PostMessage(w, WM_CLOSE, 0, 0);
        }
        return 0;
    }

    case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        PostMessage(w, WM_CLOSE, 0, 0);
        return 0;
    }
    return DefWindowProc(w, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE h, HINSTANCE p, LPSTR cmd, int show) {
    srand((unsigned)time(NULL));
    WNDCLASS wc = { 0 };
    wc.hInstance = h;
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = "MatrixSaver";
    RegisterClass(&wc);

    HWND w = CreateWindowEx(
        0, wc.lpszClassName, "Matrix", WS_POPUP,
        0, 0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        NULL, NULL, h, NULL
    );
    ShowWindow(w, SW_SHOW);
    UpdateWindow(w);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
