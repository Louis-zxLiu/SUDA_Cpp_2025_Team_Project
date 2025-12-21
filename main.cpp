#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wchar.h>
#include <vector>
#include <cmath>
#include <algorithm>

// 菜单ID定义
#define ID_START    101    // 开始游戏
#define IDM_ABOUT   102    // 关于
#define IDM_EXIT    103    // 退出

// 数据结构定义
struct Letter {
    float x, y;
    char c;
    COLORREF color;
    float speed;
};

struct Particle {
    float x, y;
    float vx, vy;
    int life; // 生命期（帧数）
    COLORREF color;
};

// 游戏全局变量
int left = 100, top = 20, right = left + 250, bottom = top + 400; // 下落区域
std::vector<Letter> g_letters;                                    // 活跃字母列表
std::vector<Particle> g_particles;                                // 活跃粒子列表
int iScoring = 0, iFail = 0;                                      // 得分/失败次数
int iHighScore = 0;                                               // 最高分
int iCombo = 0;                                                   // 连击数
int gameover = 0;                                                 // 游戏结束标志
int paused = 0;                                                   // 暂停标志
const int MAX_FAIL = 10;                                          // 最大失败次数
int g_bombs = 3;                                                  // 炸弹数量
int g_spawnTimer = 0;                                             // 生成计时器

// 函数声明
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void DrawBk(HDC hdc, int left, int top, int right, int bottom);
void ShowScoring(HDC hdc, int x, int y, int score, int fail);
void GameOver(HDC hdc, int x, int y);
void Fire(HDC hdc, int startX, int startY, int endX, int endY);
void SpawnLetter();
void SpawnParticles(float x, float y, COLORREF color);
void UseBomb(HWND hWnd);
void LoadHighScore();
void SaveHighScore();

// 主函数（Win32程序入口）
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEXW wcex;
    HWND hWnd;
    MSG msg;

    // 初始化随机数生成器
    srand((unsigned int)time(NULL));

    // 加载最高分
    LoadHighScore();

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

// 窗口过程函数
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
            switch (wmId)
            {
            case ID_START:
                // 重置游戏状态
                gameover = 0;
                iScoring = 0;
                iFail = 0;
                iCombo = 0;
                g_bombs = 3;
                g_letters.clear();
                g_particles.clear();
                g_spawnTimer = 0;
                
                // 立即生成第一个字母
                SpawnLetter();

                // 启动定时器（30ms一次，约33FPS，更流畅）
                SetTimer(hWnd, 1, 30, NULL);
                InvalidateRect(hWnd, NULL, TRUE);
                break;

            case IDM_ABOUT:
                MessageBoxW(hWnd, L"字母射击游戏\n\n特性：\n1. 多字母同时下落\n2. 粒子爆炸特效\n3. 空格键释放全屏炸弹\n4. 连击系统与得分加成\n5. 最高分记录", L"关于", MB_OK);
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
        if (wParam == 1 && !gameover && !paused)
        {
            // 1. 字母生成逻辑
            // 随着分数增加，生成频率加快
            int spawnRate = 60 - (iScoring / 10); // 初始每60帧(约1.8秒)生成一个
            if (spawnRate < 20) spawnRate = 20;   // 最快每0.6秒一个
            
            g_spawnTimer++;
            if (g_spawnTimer >= spawnRate)
            {
                SpawnLetter();
                g_spawnTimer = 0;
            }

            // 2. 更新字母位置
            for (size_t i = 0; i < g_letters.size(); )
            {
                g_letters[i].y += g_letters[i].speed;

                // 判断是否落地
                if (g_letters[i].y > bottom)
                {
                    iFail++;
                    iCombo = 0; // 落地重置连击
                    Beep(200, 100);
                    g_letters.erase(g_letters.begin() + i);

                    if (iFail >= MAX_FAIL)
                    {
                        gameover = 1;
                        if (iScoring > iHighScore) {
                            iHighScore = iScoring;
                            SaveHighScore();
                        }
                        KillTimer(hWnd, 1);
                    }
                }
                else
                {
                    i++;
                }
            }

            // 3. 更新粒子位置
            for (size_t i = 0; i < g_particles.size(); )
            {
                g_particles[i].x += g_particles[i].vx;
                g_particles[i].y += g_particles[i].vy;
                g_particles[i].vy += 0.5f; // 重力效果
                g_particles[i].life--;

                if (g_particles[i].life <= 0)
                {
                    g_particles.erase(g_particles.begin() + i);
                }
                else
                {
                    i++;
                }
            }

            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;

    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        
        // 双缓冲绘图防止闪烁 (可选优化，这里直接绘图)
        DrawBk(hdc, left, top, right, bottom);
        ShowScoring(hdc, right + 20, top + 50, iScoring, iFail);

        if (gameover)
        {
            GameOver(hdc, left + 30, top + 130);
        }
        else
        {
            // 绘制所有字母
            HFONT hFont = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
            SetBkMode(hdc, TRANSPARENT);

            for (const auto& l : g_letters)
            {
                wchar_t szTemp[2] = { (wchar_t)l.c, 0 };
                SetTextColor(hdc, l.color);
                TextOutW(hdc, (int)l.x, (int)l.y, szTemp, 1);
            }
            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);

            // 绘制所有粒子
            for (const auto& p : g_particles)
            {
                SetPixel(hdc, (int)p.x, (int)p.y, p.color);
                SetPixel(hdc, (int)p.x + 1, (int)p.y, p.color); // 稍微大一点的点
                SetPixel(hdc, (int)p.x, (int)p.y + 1, p.color);
                SetPixel(hdc, (int)p.x + 1, (int)p.y + 1, p.color);
            }
        }
        
        if (paused)
        {
             SetTextColor(hdc, RGB(255, 0, 0));
             TextOutW(hdc, left + 80, top + 150, L"已暂停 (按ESC继续)", 13);
        }
        
        EndPaint(hWnd, &ps);
        break;

    case WM_CHAR:
        if (wParam == VK_ESCAPE)
        {
            if (!gameover)
            {
                paused = !paused;
                InvalidateRect(hWnd, NULL, TRUE);
            }
            break;
        }

        if (!gameover && !paused)
        {
            // 炸弹功能 (空格键)
            if (wParam == VK_SPACE)
            {
                UseBomb(hWnd);
                break;
            }

            char inputChar = (wchar_t)wParam;
            if (inputChar >= 'a' && inputChar <= 'z') inputChar -= 32; // 转大写

            // 寻找匹配的最低的字母
            int targetIndex = -1;
            float maxY = -1000.0f;

            for (size_t i = 0; i < g_letters.size(); i++)
            {
                if (g_letters[i].c == inputChar)
                {
                    if (g_letters[i].y > maxY)
                    {
                        maxY = g_letters[i].y;
                        targetIndex = i;
                    }
                }
            }

            hdc = GetDC(hWnd);
            if (targetIndex != -1)
            {
                // 击中
                Letter& target = g_letters[targetIndex];
                
                // 射击特效
                Fire(hdc, left + 5 + (inputChar - 'A') * 9 + 4, bottom, (int)target.x + 4, (int)target.y + 12);
                
                // 粒子特效
                SpawnParticles(target.x + 4, target.y + 12, target.color);
                
                Beep(1000, 30);
                iCombo++;
                iScoring += (1 + iCombo / 5); // 连击得分加成

                // 移除字母
                g_letters.erase(g_letters.begin() + targetIndex);
            }
            else
            {
                // 未击中
                Beep(300, 50);
                iFail++;
                iCombo = 0; // 按错重置连击
                if (iFail >= MAX_FAIL)
                {
                    gameover = 1;
                    if (iScoring > iHighScore) {
                        iHighScore = iScoring;
                        SaveHighScore();
                    }
                    KillTimer(hWnd, 1);
                }
            }
            ReleaseDC(hWnd, hdc);
            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;

    case WM_DESTROY:
        KillTimer(hWnd, 1);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

void DrawBk(HDC hdc, int left, int top, int right, int bottom)
{
    // 填充背景色
    HBRUSH hBrush = CreateSolidBrush(RGB(240, 240, 240));
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    Rectangle(hdc, left, top, right, bottom);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hBrush);

    // 绘制边框
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    Rectangle(hdc, left, top, right, bottom);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    // 绘制底部字母表
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, left + 5, bottom - 20, L"ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26);
}

void ShowScoring(HDC hdc, int x, int y, int score, int fail)
{
    wchar_t szText[64];

    SetTextColor(hdc, RGB(0, 0, 128)); // 深蓝色
    swprintf_s(szText, 64, L"最高记录：%d", iHighScore);
    TextOutW(hdc, x, y - 30, szText, wcslen(szText));

    SetTextColor(hdc, RGB(0, 0, 0));
    swprintf_s(szText, 64, L"当前得分：%d", score);
    TextOutW(hdc, x, y, szText, wcslen(szText));
    
    swprintf_s(szText, 64, L"生命值：%d", MAX_FAIL - fail);
    TextOutW(hdc, x, y + 30, szText, wcslen(szText));
    
    // 炸弹数量
    SetTextColor(hdc, RGB(200, 0, 0));
    swprintf_s(szText, 64, L"全屏炸弹(Space)：%d", g_bombs);
    TextOutW(hdc, x, y + 60, szText, wcslen(szText));

    if (iCombo >= 2) {
        SetTextColor(hdc, RGB(255, 0, 0));
        swprintf_s(szText, 64, L"Combo x%d", iCombo);
        TextOutW(hdc, x, y + 100, szText, wcslen(szText));
    }

    SetTextColor(hdc, RGB(0, 0, 0));
    TextOutW(hdc, x, y + 140, L"按 ESC 暂停", 9);
}

void GameOver(HDC hdc, int x, int y)
{
    wchar_t szText[64];
    HFONT hFont = CreateFontW(36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"黑体");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    SetTextColor(hdc, RGB(255, 0, 0));
    TextOutW(hdc, x, y, L"Game Over!", 10);

    SetTextColor(hdc, RGB(0, 0, 0));
    swprintf_s(szText, 64, L"最终得分：%d", iScoring);
    TextOutW(hdc, x - 20, y + 50, szText, wcslen(szText));

    if (iScoring >= iHighScore && iScoring > 0)
    {
        SetTextColor(hdc, RGB(0, 150, 0));
        TextOutW(hdc, x - 20, y + 100, L"新纪录！", 4);
    }

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

void Fire(HDC hdc, int startX, int startY, int endX, int endY)
{
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 100, 0)); // 橙色激光
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    MoveToEx(hdc, startX, startY, NULL);
    LineTo(hdc, endX, endY);

    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

void SpawnLetter()
{
    Letter l;
    l.c = rand() % 26 + 'A';
    l.x = (float)(left + 5 + (l.c - 'A') * 9);
    l.y = (float)top;
    l.color = RGB(rand() % 200, rand() % 200, rand() % 200);
    // 基础速度 + 难度加成 (max 5.0)
    l.speed = 1.0f + (rand() % 10) / 10.0f + (iScoring / 50.0f);
    if (l.speed > 5.0f) l.speed = 5.0f;
    
    g_letters.push_back(l);
}

void SpawnParticles(float x, float y, COLORREF color)
{
    for (int i = 0; i < 10; i++)
    {
        Particle p;
        p.x = x;
        p.y = y;
        float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
        float speed = (float)(rand() % 5 + 2);
        p.vx = cos(angle) * speed;
        p.vy = sin(angle) * speed;
        p.life = 20; // 持续20帧
        p.color = color;
        g_particles.push_back(p);
    }
}

void UseBomb(HWND hWnd)
{
    if (g_bombs > 0 && !g_letters.empty())
    {
        g_bombs--;
        
        // 消除所有字母并产生特效
        for (const auto& l : g_letters)
        {
            SpawnParticles(l.x + 4, l.y + 12, RGB(255, 0, 0)); // 红色爆炸
            iScoring++; // 炸弹消除也给分
        }
        g_letters.clear();
        
        // 全屏闪烁效果 (通过InvalidateRect触发重绘实现简易版)
        HDC hdc = GetDC(hWnd);
        HBRUSH hBrush = CreateSolidBrush(RGB(255, 200, 200));
        RECT rect = { left, top, right, bottom };
        FillRect(hdc, &rect, hBrush);
        DeleteObject(hBrush);
        ReleaseDC(hWnd, hdc);
        
        Beep(500, 300); // 长音效
        InvalidateRect(hWnd, NULL, TRUE);
    }
    else
    {
        Beep(200, 100); // 无法使用音效
    }
}

void LoadHighScore()
{
    FILE* fp;
    if (fopen_s(&fp, "highscore.dat", "rb") == 0)
    {
        fread(&iHighScore, sizeof(int), 1, fp);
        fclose(fp);
    }
}

void SaveHighScore()
{
    FILE* fp;
    if (fopen_s(&fp, "highscore.dat", "wb") == 0)
    {
        fwrite(&iHighScore, sizeof(int), 1, fp);
        fclose(fp);
    }
}
