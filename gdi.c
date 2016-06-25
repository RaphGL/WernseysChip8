/*
Evolved from the HELLO_WIN.C example in tcc
References:
https://www.daniweb.com/software-development/cpp/code/241875/fast-animation-with-the-windows-gdi
https://www-user.tu-chemnitz.de/~heha/petzold/ch14e.htm
https://www-user.tu-chemnitz.de/~heha/petzold/ch15d.htm
http://forums.codeguru.com/showthread.php?487633-32-bit-DIB-from-24-bit-bitmap
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <windows.h>

#include "bmp.h"
#include "gdi.h"

static char szAppName[] = APPNAME;
static char szTitle[]   = APPNAME;

Bitmap *screen = NULL;

/* Virtual-Key Codes here:
https://msdn.microsoft.com/en-us/library/windows/desktop/dd375731(v=vs.85).aspx */
#define MAX_KEYS 256
char keys[MAX_KEYS];

static int quit;

#if EPX_SCALE
static Bitmap *scale_epx_i(Bitmap *in, Bitmap *out);
#endif

void clear_keys() {
    int i;
    for(i = 0; i < MAX_KEYS; i++) {
        keys[i] = 0;
    }
}

void exit_error(const char *msg, ...) {
    char message_text[256];
    if(msg) {
        va_list arg;
        va_start (arg, msg);
        vsnprintf (message_text, (sizeof message_text) - 1, msg, arg);
        va_end (arg);
    }
    MessageBox(
        NULL,
        message_text,
        "Error",
        MB_ICONERROR | MB_OK
    );
    exit(1);
}

/** WIN32 and GDI routines below this line *****************************************/

static int split_cmd_line(char *cmdl, char *argv[], int max);
static void FitWindow(HWND hWnd);

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HDC hdc, hdcMem;
    static HBITMAP hbmOld, hbmp;

#if EPX_SCALE
	static Bitmap *epx = NULL;
#endif

#define MAX_ARGS    16
    static int argc = 0;
    static char *argv[MAX_ARGS];
    static LPTSTR cmdl;

    switch (message) {

        case WM_CREATE: {
            unsigned char *pixels;

            BITMAPINFO bmi;
            ZeroMemory(&bmi, sizeof bmi);
            bmi.bmiHeader.biSize = sizeof(BITMAPINFO);
            bmi.bmiHeader.biWidth = VSCREEN_WIDTH;
            bmi.bmiHeader.biHeight =  -VSCREEN_HEIGHT; // Order pixels from top to bottom
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32; // last byte not used, 32 bit for alignment
            bmi.bmiHeader.biCompression = BI_RGB;
            bmi.bmiHeader.biSizeImage = 0;
            bmi.bmiHeader.biXPelsPerMeter = 0;
            bmi.bmiHeader.biYPelsPerMeter = 0;
            bmi.bmiHeader.biClrUsed = 0;
            bmi.bmiHeader.biClrImportant = 0;
            bmi.bmiColors[0].rgbBlue = 0;
            bmi.bmiColors[0].rgbGreen = 0;
            bmi.bmiColors[0].rgbRed = 0;
            bmi.bmiColors[0].rgbReserved = 0;

            hdc = GetDC( hwnd );
            hbmp = CreateDIBSection( hdc, &bmi, DIB_RGB_COLORS, (void**)&pixels, NULL, 0 );

            hdcMem = CreateCompatibleDC( hdc );
            hbmOld = (HBITMAP)SelectObject( hdcMem, hbmp );

#if !EPX_SCALE
            screen = bm_bind(VSCREEN_WIDTH, VSCREEN_HEIGHT, pixels);
#else
			screen = bm_create(SCREEN_WIDTH, SCREEN_HEIGHT);
            epx = bm_bind(VSCREEN_WIDTH, VSCREEN_HEIGHT, pixels);
#endif
            bm_set_color_s(screen, "black");
            bm_clear(screen);

            clear_keys();

            cmdl = strdup(GetCommandLine());
            argc = split_cmd_line(cmdl, argv, MAX_ARGS);

            init_game(argc, argv);

            } break;

        case WM_DESTROY:
            quit = 1;
            deinit_game();

            free(cmdl);

#if !EPX_SCALE
            bm_unbind(screen);
#else		
			bm_free(screen);
			bm_unbind(epx);
#endif
            SelectObject( hdcMem, hbmOld );
            DeleteDC( hdc );
            screen = NULL;
            PostQuitMessage(0);
            break;

        case WM_RBUTTONUP:
#if 0
            DestroyWindow(hwnd);
#endif
            break;

        case WM_KEYDOWN:
            if (wParam < MAX_KEYS) {
                keys[wParam] = 1;
            }
#if 1
            if (VK_ESCAPE == wParam)
                DestroyWindow(hwnd);
#endif
            break;

        case WM_KEYUP:
            if (wParam < MAX_KEYS) {
                keys[wParam] = 0;
            }
            break;

        case WM_PAINT:
        {
            if(!screen) break;
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint( hwnd, &ps );
			
#if EPX_SCALE
			scale_epx_i(screen, epx);
#endif
			
#if WINDOW_WIDTH == VSCREEN_WIDTH && WINDOW_HEIGHT == VSCREEN_HEIGHT
            BitBlt( hdc, 0, 0, VSCREEN_WIDTH, VSCREEN_HEIGHT, hdcMem, 0, 0, SRCCOPY );
#else
            StretchBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdcMem, 0, 0, VSCREEN_WIDTH, VSCREEN_HEIGHT, SRCCOPY);
#endif
            EndPaint( hwnd, &ps );
            break;
        }
        /* Don't erase the background - it causes flickering
            http://stackoverflow.com/a/14153470/115589 */
        case WM_ERASEBKGND:
            return 1;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

int APIENTRY WinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow
        )
{
    MSG msg;
    WNDCLASS wc;
    HWND hwnd;
    double elapsedSeconds = 0.0;

    ZeroMemory(&wc, sizeof wc);
    wc.hInstance     = hInstance;
    wc.lpszClassName = szAppName;
    wc.lpfnWndProc   = (WNDPROC)WndProc;
    wc.style         = CS_DBLCLKS|CS_VREDRAW|CS_HREDRAW;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);

    if (FALSE == RegisterClass(&wc))
        return 0;

    hwnd = CreateWindow(
        szAppName,
        szTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        //WS_POPUP, // For no border/titlebar etc
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        0,
        0,
        hInstance,
        0);

    if (NULL == hwnd)
        return 0;

    FitWindow(hwnd);

    ShowWindow(hwnd , SW_SHOW);

    /* Todo: I didn't bother with higher resolution timers:
    https://msdn.microsoft.com/en-us/library/dn553408(v=vs.85).aspx */

    quit = 0;
    for(;;) {
        clock_t startTime, endTime;
        startTime = clock();
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if(quit) break;

        Sleep(1);
        endTime = clock();
        elapsedSeconds += (double)(endTime - startTime) / CLOCKS_PER_SEC;
        if(elapsedSeconds > 1.0/FPS) {
            if(!render(elapsedSeconds)) {
                DestroyWindow(hwnd);
            }
            InvalidateRect(hwnd, 0, TRUE);
            elapsedSeconds = 0.0;
        }
    }

    return msg.wParam;
}

/* Make sure the client area fits; center the window in the process */
static void FitWindow(HWND hwnd) {
    RECT rcClient, rwClient;
    POINT ptDiff;
    HWND hwndParent;
    RECT rcParent, rwParent;
    POINT ptPos;

    GetClientRect(hwnd, &rcClient);
    GetWindowRect(hwnd, &rwClient);
    ptDiff.x = (rwClient.right - rwClient.left) - rcClient.right;
    ptDiff.y = (rwClient.bottom - rwClient.top) - rcClient.bottom;

    hwndParent = GetParent(hwnd);
    if (NULL == hwndParent)
        hwndParent = GetDesktopWindow();

    GetWindowRect(hwndParent, &rwParent);
    GetClientRect(hwndParent, &rcParent);

    ptPos.x = rwParent.left + (rcParent.right - WINDOW_WIDTH) / 2;
    ptPos.y = rwParent.top + (rcParent.bottom - WINDOW_HEIGHT) / 2;

    MoveWindow(hwnd, ptPos.x, ptPos.y, WINDOW_WIDTH + ptDiff.x, WINDOW_HEIGHT + ptDiff.y, 0);
}

/*
Alternative to CommandLineToArgvW().
I used a compiler where shellapi.h was not available,
so this function breaks it down according to the last set of rules in
http://i1.blogs.msdn.com/b/oldnewthing/archive/2010/09/17/10063629.aspx
*/
static int split_cmd_line(char *cmdl, char *argv[], int max) {

    int argc = 0;
    char *p = cmdl, *q = p, *arg = p;
    int state = 1;
    while(state) {
        switch(state) {
            case 1:
                if(argc == max) return argc;
                if(!*p) {
                    state = 0;
                } else if(isspace(*p)) {
                    *q++ = *p++;
                } else if(*p == '\"') {
                    state = 2;
                    *q++ = *p++;
                    arg = q;
                } else {
                    state = 3;
                    arg = q;
                    *q++ = *p++;
                }
            break;
            case 2:
                if(!*p) {
                    argv[argc++] = arg;
                    *q++ = '\0';
                    state = 0;
                } else if(*p == '\"') {
                    if(p[1] == '\"') {
                        state = 2;
                        *q++ = *p;
                        p+=2;
                    } else {
                        state = 1;
                        argv[argc++] = arg;
                        *q++ = '\0';
                        p++;
                    }
                } else {
                    *q++ = *p++;
                }
            break;
            case 3:
                if(!*p) {
                    state = 0;
                    argv[argc++] = arg;
                    *q++ = '\0';
                } else if(isspace(*p)) {
                    state = 1;
                    argv[argc++] = arg;
                    *q++ = '\0';
                    p++;
                } else {
                    *q++ = *p++;
                }
            break;
        }
    }
    return argc;
}

/* EPX 2x scaling */
#if EPX_SCALE
static Bitmap *scale_epx_i(Bitmap *in, Bitmap *out) {
	int x, y, mx = in->w, my = in->h;
	if(!out) return NULL;
	if(!in) return out;
	if(out->w < (mx << 1)) mx = (out->w - 1) >> 1;
	if(out->h < (my << 1)) my = (out->h - 1) >> 1;
	for(y = 0; y < my; y++) {
		for(x = 0; x < mx; x++) {
			unsigned int P = bm_get(in, x, y);
			unsigned int A = (y > 0) ? bm_get(in, x, y - 1) : P;
			unsigned int B = (x < in->w - 1) ? bm_get(in, x + 1, y) : P;
			unsigned int C = (x > 0) ? bm_get(in, x - 1, y) : P;
			unsigned int D = (y < in->h - 1) ? bm_get(in, x, y + 1) : P;
			
			unsigned int P1 = P, P2 = P, P3 = P, P4 = P;
								
			if(C == A && C != D && A != B) P1 = A;
			if(A == B && A != C && B != D) P2 = B;
			if(B == D && B != A && D != C) P4 = D;
			if(D == C && D != B && C != A) P3 = C;
						
			bm_set(out, (x << 1), (y << 1), P1);
			bm_set(out, (x << 1) + 1, (y << 1), P2);
			bm_set(out, (x << 1), (y << 1) + 1, P3);
			bm_set(out, (x << 1) + 1, (y << 1) + 1, P4);
		}
	}
	return out;
}
#endif