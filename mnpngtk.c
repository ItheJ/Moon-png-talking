//Приложение Moon PNG Talking версии 1.0

//библиотеки raylib'а

#include "./raylib/raylib.h"
#include "raylib/rlgl.h"
#include "raylib/raymath.h"

#define PlaySound Win32PlaySoundA // заготовка для подмены функции на ту же из WinAPI

//всё для включения WinAPI
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOMINMAX

#define ShowCursor Win32ShowCursor //такие же заготовки, но для подмены на те же из Raylib
#define HideCursor Win32HideCursor
#define CloseWindow Win32CloseWindow
#define LoadImage Win32LoadImage
#define DrawTextA Win32DrawTextA
#define DrawTextExA Win32DrawTextExA

#include <windows.h>
#include <mmsystem.h> //для работы с микрофоном

//подмена
#undef PlaySound

#undef ShowCursor
#undef HideCursor
#undef CloseWindow
#undef LoadImage
#undef DrawTextA
#undef DrawTextExA

//Остальные библиотеки

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//Частота микрофона
#define SAMPLE_RATE 44100
//Объём буфера микрофона
#define MICROBUFFER_SIZE 1024

//команда компиляции
//windres mnpngtk.rc -O coff -o mnpngtkrs.o
//gcc mnpngtk.c mnpngtkrs.o -o mnpngtk.exe -lraylib -lopengl32 -lgdi32 -lwinmm -lkernel32 -lshell32 -luser32 -mwindows

//Целочисленный аналог Vector2
typedef struct {
	int x;
	int y;
} ZVector2;
/*
Структура User - содержит данные для работы приложения:
wh - длина и ширина окна
mouth - где будет располагаться рот (точка)
mouth_wh - длина и максимальная высота рта

imgname - название файла с изображением
*/
typedef struct {
	ZVector2 wh;
	ZVector2 mouth;
	ZVector2 mouth_wh;
	
	char imgname[255];
} User;


//Захват окном фокуса
void GetWinFocus(HWND hwnd);
//Чтение данных с файла настрое
User *ReadUsrData();
//Отрисовка фона
void DrawSb();
//Получение уровня громкости микрофона
double ParseVolMicro();
//Отрисовка рта
void DrawMouth(Vector2 center, float maxW, float maxH, float open, Color color);

//Количество кадров в секунду (уменьшенно чтобы меньше нагружать компьютер)
const unsigned int FPS = 30;

//Требуется ли отрисовка окна
bool req_draw = true;
//Переменная для получения новой позиции окна через мышь
POINT point;

//Переменная со структурой
static User usr;

/*Переменные для работы с микрофоном
Дескриптор
Формат звукового потока
Идентификатор буфера микрофона
*/
static HWAVEIN hWaveIn;
static WAVEFORMATEX wfx;
static WAVEHDR waveHdr;

//Присутствует ли в микрофоне ошибка
int error = 0;
// 0 - ok
// 1 - no micro
// 2 - error with buffer

//буфер микрофона
static short microbuffer[MICROBUFFER_SIZE];
//Используемая для отрисовки текстура
Texture2D face;


int main(void){
	//Инициализация значений по умолчанию
	usr.wh = (ZVector2){
		.x = 400,
		.y = 400
	};
	usr.mouth = (ZVector2){
		.x = 200,
		.y = 200
	};
	usr.mouth_wh = (ZVector2){
		.x = 200,
		.y = 50
	};
	strncpy(usr.imgname, "./images/blank.png\0", 19);
	
	//Чтение данных из файла в указатель и присвоение его значения
	User *newusrp = ReadUsrData();
	usr = *newusrp;
	
	//Инициализация
	wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = SAMPLE_RATE;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;
	// Проверка на наличие микрофона
	if (waveInOpen(&hWaveIn, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR){
        error = 1;
    }
	else {
		//Инициализация буфера
		waveHdr.lpData = (LPSTR)microbuffer;
		waveHdr.dwBufferLength = MICROBUFFER_SIZE * sizeof(short);
		waveHdr.dwBytesRecorded = 0;
		waveHdr.dwUser = 0;
		waveHdr.dwFlags = 0;
		waveHdr.dwLoops = 0;
		
		//Проверка на успешность инициализации буфера
		if (waveInPrepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
			error = 2;
			//Закрывает поток микрофона - но программа может работать и без него
			waveInClose(hWaveIn);
		}
		else {
			//Начало записи микрофона
			waveInStart(hWaveIn);
		}
	}
	//Сделать фреймбуфер прозрачным и удаляет рамку окна и мышь может проходить сквозь окно кликами мыши
	SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_MOUSE_PASSTHROUGH);
	
	//Создание окна
	InitWindow(usr.wh.x, usr.wh.y, "your_cute_face_miii!");
	//Установка лимита на кадры в секунду
	SetTargetFPS(FPS);
	//Отключает выход по кнопке
	SetExitKey(0);
	
	//Подгрузка иконки для окна
	Image ico = LoadImage("./images/flower.png");
	SetWindowIcon(ico); // Установка иконки
	UnloadImage(ico); //Её выгрузка
	
	//Окно становится поверх остальных
	SetWindowState(FLAG_WINDOW_TOPMOST);
	
	//Загрузка текстуры
	face = LoadTexture(usr.imgname);

	//Пока окно не будет закрыто, оно будет работать
	while (!WindowShouldClose()){
		
		if (GetAsyncKeyState(VK_HOME) & 0x8000){
			//Обработка нажатия клавиши HOME в любое время (удобно когда окно не в фокусе)
			
			if (!req_draw){
				//При возвращении окна оно обязательно будет в фокусе
				GetWinFocus((HWND)GetWindowHandle());
			}
			//Меняем значение на противоположное
			req_draw = !req_draw;
			//Небольшая задержка
			Sleep(100);
		}
		
		if (req_draw){
			//Если окно есть - оно будет обрабатывать TAB и ESCAPE
			if (IsKeyPressed(KEY_TAB)){
				//Устанавливаем окно на то место, где сейчас указатель мыши
				GetCursorPos(&point);
				SetWindowPosition(point.x, point.y);
			}
			if (IsKeyPressed(KEY_ESCAPE)){
				//Выход из программы...
				break;
			}
		}
		
		//Начало отрисовки
		BeginDrawing();
			//Сделать фон прозрачным
			ClearBackground(BLANK);
			if (req_draw){
				//Отрисовка лица
				DrawSb();
				
				//Отрисовка рта (в виде линии и в виде дуги)
				DrawLine(usr.mouth.x - (usr.mouth_wh.x / 2), usr.mouth.y, usr.mouth.x + (usr.mouth_wh.x / 2), usr.mouth.y, BLACK);
				if (!error){
					//Делается только если с микрофоном всё хорошо
					DrawMouth((Vector2){usr.mouth.x, usr.mouth.y}, usr.mouth_wh.x, usr.mouth_wh.y, ParseVolMicro(), BLACK);
				}
			}
		//Отрисовка окончена
		EndDrawing();
	}

	if (!error){
		//При отсутствии ошибок - завершение всего, связанного с микрофоном 
		waveInStop(hWaveIn);
		waveInUnprepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR));
		waveInClose(hWaveIn);
	}
	
	//Выгрузка текстуры
	UnloadTexture(face);
	//Закрытие окна
	CloseWindow();
	
	//Здесь программа завершает свою работу...
	return 0;
}

/*Функция фокуса окна
Суть - сделать так, чтобы окно было активным
Аргумент - дескриптор нужного окна
Возвращаемый результат - отсутствует
*/
void GetWinFocus(HWND hwnd){
	// Получение ID потока текущего окна и нашего
	int foreThrID = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
	int curThrID = GetCurrentThreadId();

	if (foreThrID != curThrID){
		//При несовпадении - связывает нажатия клавиши между окнами, делает нужное окно активным и отвязывает
		AttachThreadInput(curThrID, foreThrID, true);
		
		SetForegroundWindow(hwnd);
		SetFocus(hwnd);
		
		AttachThreadInput(curThrID, foreThrID, false);
	} else {
		//Иначе - просто делает активным
		SetForegroundWindow(hwnd);
		SetFocus(hwnd);
	}
}

/*Функция чтения файла настроек
Суть - получить информацию из файла usrsettings.txt и сделать это безопасно
Аргументов нет
Возвращаемый результат - указатель на результат
*/
User *ReadUsrData(){
	User *userp = &usr;
	
	//Открываем файл в режиме чтения
	FILE *f = fopen("usrsettings.txt", "r");
	
	//Файл отсутствует? Значит записываем значения по умолчанию и возвращаем указатель на исходную структуру
	if (!f){
		FILE *f = fopen("usrsettings.txt", "w");
		fprintf(f, "[USER]\nwidthscreen = %d\nheightscreen = %d\nmouthposx = %d\nmouthposy = %d\nmouthwidth = %d\nmouthheight = %d\nfilename = %s", usr.wh.x, usr.wh.y, usr.mouth.x, usr.mouth.y, usr.mouth_wh.x, usr.mouth_wh.y, usr.imgname);
		fclose(f);
		return (&usr);
	}
	// Если есть - читаем
	else {
		//Буфер строки
		char line[512];
		
		//Построчное чтение из файла и сверка с наличием строк с заданными значениями
		while (fgets(line, sizeof(line), f)){
			line[strcspn(line, "\n")] = '\0';
			if (sscanf(line, "widthscreen = %d", &userp->wh.x) == 1){
				if (!userp->wh.x) userp->wh.x = usr.wh.x;
				continue;
			}
			if (sscanf(line, "heightscreen = %d", &userp->wh.y) == 1){
				if (!userp->wh.y) userp->wh.y = usr.wh.y;
				continue;
			}
			
			if (sscanf(line, "mouthposx = %d", &userp->mouth.x) == 1){
				if (!userp->mouth.x) userp->mouth.x = usr.mouth.x;
				continue;
			}
			if (sscanf(line, "mouthposy = %d", &userp->mouth.y) == 1){
				if (!userp->mouth.y) userp->mouth.y = usr.mouth.y;
				continue;
			}
			
			if (sscanf(line, "mouthwidth = %d", &userp->mouth_wh.x) == 1){
				if (!userp->mouth_wh.x) userp->mouth_wh.x = usr.mouth_wh.x;
				continue;
			}
			if (sscanf(line, "mouthheight = %d", &userp->mouth_wh.y) == 1){
				if (!userp->mouth_wh.y) userp->mouth_wh.y = usr.mouth_wh.y;
				continue;
			}
			
			if (sscanf(line, "filename = %254s", &userp->imgname) == 1){
				continue;
			}
		}
	}
	
	//Возврат из функции...
	return userp;
}
/*Функция отрисовки нужной текстуры
Суть - отрисовка текстуры ак, чтобы она была во всё окно и не смещена от этого
Аргументы - нету
Возвраемый результат - отсутствует
*/
void DrawSb(){
	// Отключение проверки глубины и запрет скрывать полигоны от камеры
	rlDisableDepthTest();
	rlDisableBackfaceCulling();

	//Представление текстуры и окна как прямоугольников и отрисовка с учётом масштаба src текстуры face на dst

	Rectangle src = {0.0f, 0.0f, (float)face.width, (float)face.height};
	Rectangle dst = {0.0f, 0.0f, (float)usr.wh.x, (float)usr.wh.y};

	DrawTexturePro(face, src, dst, (Vector2){0,0}, 0.0f, WHITE);
	
	// Включение проверки глубины и разрешение скрывать полигоны от камеры
	rlEnableBackfaceCulling();
	rlEnableDepthTest();
}

/*Функция обработки звука от микрофона
Суть - получаем значение громкости микрофона
Аргументы - нету
Возвращаемый результат - переменная типа double (число с плавающей точкой двойной точности)
*/
double ParseVolMicro(){
	//Отправка буфера на запись звука
	waveInAddBuffer(hWaveIn, &waveHdr, sizeof(WAVEHDR));
	
	//Ждём готовности микрофона
	while (!(waveHdr.dwFlags & WHDR_DONE)){
		Sleep(5);
	}

	int maxVal = 0;
	// количество сэмплов
	int sampleCount = waveHdr.dwBytesRecorded / sizeof(short);
	
	//нахождение пикового значения громкости
	for (int i = 0; i < sampleCount; i++){
		int absVal = abs(microbuffer[i]);
		if (absVal > maxVal){
			maxVal = absVal;
		}
	}

	//расчёт одного процента громкости
	double volumePercent = ((double)maxVal / 32767.0);
	//Сброс флага готовности
	waveHdr.dwFlags &= ~WHDR_DONE;
	
	//Возврат из функции...
	return volumePercent;
}

/*Функция отрисовки рта
Суть - отрисовать динамическую дугу
Аргументы - центр дуги, максимальное значение длины и высоты дуги, насколько открыта дуга, цвет
Возвращаемый результат - отсутствует
*/
void DrawMouth(Vector2 center, float maxW, float maxH, float open, Color color){
	//Не отрисовываем в случае если рот \"не открыт\"
	if (open <= 0.0f) return;

	//Нужные для работы значения
	float halfW = maxW * 0.5f;
	float h = maxH * open;

	//Сколько должно быть сегментов у рта
	int segments = 32;
	
	for (int i = 0; i < segments; i++){
		//Пересчёт сегментов и выведение нужных точек, из которых, по итогу, будут построены залитые треугольники
		float a0 = PI * (float)i / segments;
		float a1 = PI * (float)(i + 1) / segments;

		Vector2 p0 = {center.x + cosf(a0) * halfW, center.y + sinf(a0) * h};
		Vector2 p1 = {center.x + cosf(a1) * halfW, center.y + sinf(a1) * h};

		DrawTriangle(p1, p0, center, color);
    }
}
/*
Написал программу буквально по фану за выходные, надеюсь вам было интересно прочитать эти все комментарии (я старался честно ;) )
Программа сделана ItheJ (a.k.a Zondrobonie), 2026 год.
Стабильная версия от 13 ноября

Программа разрабатывалась на открытой библиотеке Raylib версии 6.1-dev и 5.5 в динамической библиотеке
*/