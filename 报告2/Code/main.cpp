#include <Windows.h>
#include <tchar.h>
#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <time.h>


using namespace std;

#define BUF_SIZE (sizeof(double) << 20)
#define NUM_RANDOM_NUMBERS 1000000			//随机数总数
#define MAX_NUM_PROC 25						//最多进程数

//读取随机数文件
bool read_random_number(string file_pth, double *random_numbers) {
	ifstream in;
	in.open(file_pth, ios::in);
	if (!in.is_open()) {
		cout << "- 无法打开随机数文件 -" << '\n';
		return false;
	}

	//随机数文件为 1000行 * 1000个数/每行，每个数保存为12个字符的字符串，如"0.7790856164"
	char buffer[15000];
	cout << "- 读取随机数中 -" << '\n';
	for(int i = 0; i < 1000; i++)
	{
		//读取一行
		in.getline(buffer, 15000, '\n');
		
		//转化为double型
		char str_number[13];
		for (int j = 0; j < 1000; j++) {
			for (int k = 0; k < 12; k++) {
				str_number[k] = buffer[j * 13 + k];
				sscanf(str_number, "%12lf", &(random_numbers[1000 * i + j]));
			}
		}
	}

	cout << "- 读取随机数文件完成 -" << '\n';
	in.close();
	return true;
}

void save_sorted_random_number(string file_pth, double *random_numbers) {
	ofstream out;
	out.open(file_pth, ios::out);

	cout << "- 写入已排序随机数中 -" << '\n';
	char buffer[13];
	for (int i = 0; i < 1000; i++) {
		for (int j = 0; j < 1000; j++) {
			sprintf(buffer, "%12lf", random_numbers[1000 * i + j]);
			out << buffer << ' ';
		}
		out << '\n';
	}
	out.close();

	cout << "- 写入已排序随机数完成 -" << '\n';
}

int main() {
	//1.创建共享内存区域
	TCHAR hMapName[] = TEXT("NumberRegion");
	HANDLE hMapFile = CreateFileMapping(
		INVALID_HANDLE_VALUE,				//共享内存
		NULL,
		PAGE_READWRITE,						//可读可写
		0,						
		BUF_SIZE,							//BUF_SIZE MB				
		hMapName);

	if (hMapFile == NULL)
	{
		DWORD error_code = GetLastError();
		cout << "- 无法创建文件映射对象, 错误码: " << error_code << " -" << '\n';
		system("pause");
		return 1;
	}
	cout << "- 创建文件映射对象完成 -" << '\n';


	//2.内存映射
	double* pBuf = (double*)MapViewOfFile(hMapFile,   
		FILE_MAP_ALL_ACCESS,				//可读可写
		0,
		0,
		BUF_SIZE);							//BUF_SIZE MB

	if (pBuf == NULL)
	{
		DWORD error_code = GetLastError();
		cout << "- 无法创建内存映射文件, 错误码: " << error_code << " -" << '\n';
		CloseHandle(hMapFile);
		system("pause");
		return 1;
	}
	cout << "- 创建内存映射文件完成 -" << '\n';


	//*****************************
	//*使用时请自行修改路径，谢谢~*
	//*****************************
	double *random_numbers = new double[NUM_RANDOM_NUMBERS];
	if (!read_random_number(".\\random_numbers.txt", random_numbers)) {
		UnmapViewOfFile(pBuf);
		CloseHandle(hMapFile);
		system("pause");
		return 1;
	}
	
	//3.将数组复制进共享内存
	CopyMemory((PVOID)pBuf, random_numbers, sizeof(double) * NUM_RANDOM_NUMBERS);


	//4.创建用于控制进程数目的信号量
	TCHAR hSemName[] = TEXT("MaxNumProc");
	HANDLE hSemaphore = CreateSemaphore(
		NULL,
		MAX_NUM_PROC,
		MAX_NUM_PROC,
		hSemName
	);

	if (hSemaphore == NULL)
	{
		DWORD error_code = GetLastError();
		cout << "- 无法创建信号量, 错误码: " << error_code << " -" << '\n';
		delete[] random_numbers;
		UnmapViewOfFile(pBuf);
		CloseHandle(hMapFile);
		system("pause");
		return 1;
	}


	//5.创建子进程
	clock_t start, finish;
	wchar_t lpCommandLine[] = L".\\subsort.exe NumberRegion MaxNumProc 0 999999";
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	start = clock();
	WaitForSingleObject(hSemaphore, INFINITE);
	bool cp = CreateProcess(
		NULL,        
		lpCommandLine,
		NULL,
		NULL,
		FALSE,
		0,
		NULL,
		NULL,
		&si,
		&pi);

	if (!cp) {
		DWORD error_code = GetLastError();
		cout << "- 无法创建子进程, 错误码: " << error_code << " -" << '\n';
		delete[] random_numbers;
		CloseHandle(hSemaphore);
		UnmapViewOfFile(pBuf);
		CloseHandle(hMapFile);
		system("pause");
		return 1;
	}

	cout << "- 多进程快速排序开始 -" << '\n';

	//等待所有子进程结束
	DWORD dwWaitResult;
	LONG lpPrevCount;
	while (true) { 
		dwWaitResult = WaitForSingleObject(hSemaphore, INFINITE);	
		if (dwWaitResult == WAIT_OBJECT_0) {
			ReleaseSemaphore(hSemaphore, 1, &lpPrevCount);
			if (lpPrevCount == (MAX_NUM_PROC - 1)) break;
		}
		Sleep(100); 
	}
	finish = clock();
	cout << "- 多进程快速排序结束, 耗时: " << (double)(finish - start)/CLOCKS_PER_SEC << "s -" << '\n';


	//将排序后的随机数写入文件
	save_sorted_random_number(".\\sorted_numbers_cpp.txt", pBuf);

	delete[] random_numbers;
	CloseHandle(hSemaphore);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	UnmapViewOfFile(pBuf);
	CloseHandle(hMapFile);

	system("pause");

	return 0;
}