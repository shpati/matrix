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
#define FRAME_INTERVAL   40   // ms per frame 
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

typedef struct {
    HDC hdc, memdc;
    HBITMAP bmp;
    HGDIOBJ oldbmp;
    HFONT hFont;
    Drop drops[MAX_COLS];
    int ncols, width, height;
} MATRIXSTATE;

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
        float t = (drops[i].speed - FALL_SPEED_MIN) / (float)(FALL_SPEED_MAX - FALL_SPEED_MIN);
        drops[i].refreshDelay = (int)(LETTER_CYCLE_MAX - t * (LETTER_CYCLE_MAX - LETTER_CYCLE_MIN));
        drops[i].refreshCounter = rnd(0, drops[i].refreshDelay);
        drops[i].burst = (rand() % BURST_CHANCE == 0);

        for (int j = 0; j < MAX_TRAIL; j++)
            drops[i].chars[j] = charset[rand() % (sizeof(charset) / sizeof(WCHAR) - 1)];
    }
}

void draw_frame(HDC memdc) {
    RECT r = { 0, 0, width, height };
    FillRect(memdc, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SetBkMode(memdc, TRANSPARENT);

    for (int i = 0; i < ncols; i++) {
        Drop *d = &drops[i];

        if (--d->refreshCounter <= 0) {
            int visible = d->trail < MAX_TRAIL ? d->trail : MAX_TRAIL;
            for (int j = 0; j < visible; j++)
                d->chars[j] = charset[rand() % (sizeof(charset)/sizeof(WCHAR) - 1)];
            d->refreshCounter = d->refreshDelay;
        }

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

        d->y += d->speed;

        if (d->y - d->trail * colH > height) {
            d->y = (float)rnd(-height * 0.25f, 0);
            d->trail = rnd(TRAIL_LEN_MIN, TRAIL_LEN_MAX);
            d->burst = (rand() % BURST_CHANCE == 0);
        }
    }
}

LRESULT CALLBACK WndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    static int lastMouseX = -1, lastMouseY = -1;

    switch (msg) {
    case WM_CREATE: {
        ShowCursor(FALSE);

        MATRIXSTATE *st = (MATRIXSTATE*)calloc(1, sizeof(MATRIXSTATE));
        SetWindowLongPtr(w, GWLP_USERDATA, (LONG_PTR)st);

        st->hdc = GetDC(w);
        RECT r; GetClientRect(w, &r);
        st->width = r.right; st->height = r.bottom;

        st->hFont = CreateFontW(
            colH, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH, L"Consolas"
        );

        st->bmp = CreateCompatibleBitmap(st->hdc, st->width, st->height);
        st->memdc = CreateCompatibleDC(st->hdc);
        st->oldbmp = SelectObject(st->memdc, st->bmp);
        SelectObject(st->memdc, st->hFont);

        SIZE s;
        GetTextExtentPoint32W(st->memdc, L"W", 1, &s);
        if (s.cx > 4 && s.cy > 4) { colW = s.cx; colH = s.cy; }

        RECT full = { 0, 0, st->width, st->height };
        FillRect(st->memdc, &full, (HBRUSH)GetStockObject(BLACK_BRUSH));

        width = st->width;
        height = st->height;
        init_drops(w);
        memcpy(st->drops, drops, sizeof(drops));

        SetTimer(w, 1, FRAME_INTERVAL, NULL);
        return 0;
    }

    case WM_TIMER: {
        MATRIXSTATE *st = (MATRIXSTATE*)GetWindowLongPtr(w, GWLP_USERDATA);
        if (!st) break;

        width = st->width; height = st->height;
        memcpy(drops, st->drops, sizeof(drops));

        draw_frame(st->memdc);
        memcpy(st->drops, drops, sizeof(drops));

        BitBlt(st->hdc, 0, 0, st->width, st->height, st->memdc, 0, 0, SRCCOPY);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int x = LOWORD(lp), y = HIWORD(lp);
        if (lastMouseX == -1 && lastMouseY == -1) {
            lastMouseX = x; lastMouseY = y;
        } else if (abs(x - lastMouseX) > 3 || abs(y - lastMouseY) > 3) {
            PostMessage(w, WM_CLOSE, 0, 0);
        }
        return 0;
    }

    case WM_DESTROY: {
        MATRIXSTATE *st = (MATRIXSTATE*)GetWindowLongPtr(w, GWLP_USERDATA);
        if (st) {
            if (st->memdc) { SelectObject(st->memdc, st->oldbmp); DeleteDC(st->memdc); }
            if (st->bmp) DeleteObject(st->bmp);
            if (st->hFont) DeleteObject(st->hFont);
            if (st->hdc) ReleaseDC(w, st->hdc);
            free(st);
        }
        ShowCursor(TRUE);
        PostQuitMessage(0);
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

// Create one fullscreen Matrix window per monitor
BOOL CALLBACK MonitorEnumProc(HMONITOR hMon, HDC hdcMon, LPRECT rcMon, LPARAM lp) {
    HINSTANCE h = (HINSTANCE)lp;
    WNDCLASS wc = { 0 };
    wc.hInstance = h;
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = "MatrixSaver";
    RegisterClass(&wc);

    HWND w = CreateWindowEx(
        0, wc.lpszClassName, "Matrix", WS_POPUP,
        rcMon->left, rcMon->top,
        rcMon->right - rcMon->left,
        rcMon->bottom - rcMon->top,
        NULL, NULL, h, NULL
    );

    ShowWindow(w, SW_SHOW);
    UpdateWindow(w);
    return TRUE;
}

int WINAPI WinMain(HINSTANCE h, HINSTANCE p, LPSTR cmd, int show) {
    srand((unsigned)time(NULL));
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)h);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
