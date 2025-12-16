#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

// 菜单ID定义
#define ID_START    101    // 开始游戏
#define IDM_ABOUT   102    // 关于
#define IDM_EXIT    103    // 退出

// 游戏全局变量
int left = 100, top = 20, right = left + 250, bottom = top + 400; // 下落区域
char c1 = 'A', c2;                                              // 下落字母/用户输入字母
int x = -1, y = -1;                                             // 字母坐标（-1为未开始）
int iScoring = 0, iFail = 0;                                    // 得分/失败次数
int gameover = 0;                                               // 游戏结束标志
int paused = 0;                                                 // 暂停标志
COLORREF charColor = RGB(0, 0, 0);                              // 当前字母颜色
const int MAX_FAIL = 10;                                        // 最大失败次数（超过则游戏结束）

// 函数声明
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void DrawBk(HDC hdc, int left, int top, int right, int bottom); // 绘制背景
void ShowScoring(HDC hdc, int x, int y, int score, int fail);   // 显示分数
void GameOver(HDC hdc, int x, int y);                           // 显示游戏结束
void Fire(HDC hdc, int x, int top, int bottom);                 // 绘制射击效果

// 主函数（Win32程序入口）
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEXW wcex;
    HWND hWnd;
    MSG msg;

    // 初始化随机数生成器
    srand((unsigned int)time(NULL));

    // 注册窗口类
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = L"LetterShootClass";
    wcex.hIconSm        = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassExW(&wcex))
    {
        MessageBoxW(NULL, L"窗口类注册失败！", L"错误", MB_ICONERROR);
        return 1;
    }

    // 创建应用程序窗口
    hWnd = CreateWindowW(
        L"LetterShootClass",
        L"字母下落射击游戏",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 500,
        NULL, NULL, hInstance, NULL);

    if (!hWnd)
    {
        MessageBoxW(NULL, L"窗口创建失败！", L"错误", MB_ICONERROR);
        return 1;
    }

    // 显示并更新窗口
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // 消息循环
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}

// 窗口过程函数（处理所有消息）
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HMENU hMenu;
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message)
    {
    case WM_CREATE:
        {
            // 创建菜单
            hMenu = CreateMenu();
            HMENU hSubMenu = CreatePopupMenu();
            AppendMenuW(hSubMenu, MF_STRING, ID_START, L"开始游戏(&S)");
            AppendMenuW(hSubMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hSubMenu, MF_STRING, IDM_ABOUT, L"关于(&A)");
            AppendMenuW(hSubMenu, MF_STRING, IDM_EXIT, L"退出(&E)");
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubMenu, L"游戏(&G)");
            SetMenu(hWnd, hMenu);
        }
        break;

    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 分析菜单选择
            switch (wmId)
            {
            case ID_START:
                // 重置游戏状态
                gameover = 0;
                iScoring = 0;
                iFail = 0;
                // 生成第一个字母
                c1 = rand() % 26 + 'A';
                charColor = RGB(rand() % 200, rand() % 200, rand() % 200); // 随机颜色(避免太浅)
                x = left + 5 + (c1 - 'A') * 9;
                y = top;
                // 启动下落定时器（100ms一次，控制下落速度）
                SetTimer(hWnd, 1, 100, NULL);
                // 刷新窗口
                InvalidateRect(hWnd, NULL, TRUE);
                break;

            case IDM_ABOUT:
                MessageBoxW(hWnd, L"字母下落射击游戏\n按对应字母键击中下落字母得分，失败10次游戏结束！", L"关于", MB_OK);
                break;

            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;

            default:
                return DefWindowProcW(hWnd, message, wParam, lParam);
            }
        }
        break;

    case WM_TIMER:
        // 处理定时器消息（字母下落）
        if (wParam == 1 && !gameover && !paused)
        {
            // 速度随分数增加而增加
            int speed = 5 + iScoring / 5;
            y += speed; 
            // 判断字母是否落地
            if (y > bottom)
            {
                iFail++; // 失败次数+1
                // 播放失败音效
                Beep(200, 100);

                // 判断是否达到最大失败次数
                if (iFail >= MAX_FAIL)
                {
                    gameover = 1;
                    KillTimer(hWnd, 1); // 停止下落定时器
                }
                else
                {
                    // 生成新字母
                    c1 = rand() % 26 + 'A';
                    charColor = RGB(rand() % 200, rand() % 200, rand() % 200);
                    x = left + 5 + (c1 - 'A') * 9;
                    y = top;
                }
                // 刷新窗口
                InvalidateRect(hWnd, NULL, TRUE);
            }
        }
        break;

    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        // 绘制背景
        DrawBk(hdc, left, top, right, bottom);
        // 显示分数和失败次数
        ShowScoring(hdc, right + 20, top + 50, iScoring, iFail);
        // 判断游戏是否结束
        if (gameover)
        {
            GameOver(hdc, left + 30, top + 130);
        }
        else if (x != -1 && y != -1)
        {
            // 显示当前下落字母
            wchar_t szTemp[8];
            swprintf_s(szTemp, sizeof(szTemp)/sizeof(wchar_t), L"%c", c1);
            SetTextColor(hdc, charColor); // 设置随机颜色
            // 字体加粗一点
            HFONT hFont = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
            TextOutW(hdc, x, y, szTemp, wcslen(szTemp));
            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);
        }
        
        // 显示暂停状态
        if (paused)
        {
             SetTextColor(hdc, RGB(255, 0, 0));
             TextOutW(hdc, left + 80, top + 150, L"已暂停 (按ESC继续)", 13);
        }
        
        EndPaint(hWnd, &ps);
        break;

    case WM_CHAR:
        // 处理暂停快捷键 (ESC键的ASCII码为27)
        if (wParam == VK_ESCAPE)
        {
            if (!gameover && x != -1) // 游戏进行中才允许暂停
            {
                paused = !paused;
                InvalidateRect(hWnd, NULL, TRUE);
            }
            break;
        }

        // 处理键盘输入（不区分大小写）
        if (!gameover && !paused)
        {
            c2 = (wParam >= 'a' && wParam <= 'z') ? (wParam - 'a' + 'A') : wParam;
            hdc = GetDC(hWnd);

            // 绘制射击效果
            Fire(hdc, left + 5 + (c2 - 'A') * 9 + 4, top, bottom);

            // 判断是否击中
            if (c2 == c1)
            {
                // 播放击中音效
                Beep(1000, 50); 
                
                // 生成新字母
                c1 = rand() % 26 + 'A';
                charColor = RGB(rand() % 200, rand() % 200, rand() % 200);
                x = left + 5 + (c1 - 'A') * 9;
                y = top;
                // 得分+1
                iScoring++;
            }
            else
            {
                // 播放未击中音效
                Beep(300, 100);

                // 未击中，失败次数+1
                iFail++;
                // 判断是否游戏结束
                if (iFail >= MAX_FAIL)
                {
                    gameover = 1;
                    KillTimer(hWnd, 1);
                }
            }

            ReleaseDC(hWnd, hdc);
            // 刷新窗口
            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;

    case WM_DESTROY:
        // 清理定时器
        KillTimer(hWnd, 1);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 绘制游戏背景（带边框的矩形区域）
void DrawBk(HDC hdc, int left, int top, int right, int bottom)
{
    // 填充背景色（浅灰色）
    HBRUSH hBrush = CreateSolidBrush(RGB(240, 240, 240));
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    Rectangle(hdc, left, top, right, bottom);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hBrush);

    // 绘制边框（黑色）
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    Rectangle(hdc, left, top, right, bottom);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    // 绘制底部字母表
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, left + 5, bottom - 20, L"ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26);
}

// 显示得分和失败次数
void ShowScoring(HDC hdc, int x, int y, int score, int fail)
{
    wchar_t szScore[64];
    // 绘制得分
    swprintf_s(szScore, sizeof(szScore)/sizeof(wchar_t), L"得分：%d", score);
    TextOutW(hdc, x, y, szScore, wcslen(szScore));
    // 绘制失败次数
    swprintf_s(szScore, sizeof(szScore)/sizeof(wchar_t), L"失败次数：%d/%d", fail, MAX_FAIL);
    TextOutW(hdc, x, y + 30, szScore, wcslen(szScore));
    
    // 绘制等级
    int level = score / 10 + 1;
    swprintf_s(szScore, sizeof(szScore)/sizeof(wchar_t), L"当前等级：%d", level);
    TextOutW(hdc, x, y + 60, szScore, wcslen(szScore));
    
    // 提示
    TextOutW(hdc, x, y + 120, L"按 ESC 暂停", 9);
}

// 显示游戏结束画面
void GameOver(HDC hdc, int x, int y)
{
    wchar_t szGameOver[64];
    // 设置字体（加大加粗）
    HFONT hFont = CreateFontW(
        36, 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"黑体");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    // 绘制游戏结束文字
    SetTextColor(hdc, RGB(255, 0, 0));
    swprintf_s(szGameOver, sizeof(szGameOver)/sizeof(wchar_t), L"Game Over!");
    TextOutW(hdc, x, y, szGameOver, wcslen(szGameOver));

    // 绘制最终得分
    SetTextColor(hdc, RGB(0, 0, 0));
    swprintf_s(szGameOver, sizeof(szGameOver)/sizeof(wchar_t), L"最终得分：%d", iScoring);
    TextOutW(hdc, x - 20, y + 50, szGameOver, wcslen(szGameOver));

    // 恢复字体
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

// 绘制射击效果（竖线模拟子弹）
void Fire(HDC hdc, int x, int top, int bottom)
{
    // 设置红色画笔
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    // 绘制竖线（子弹轨迹）
    MoveToEx(hdc, x, top, NULL);
    LineTo(hdc, x, bottom);

    // 恢复画笔
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}
