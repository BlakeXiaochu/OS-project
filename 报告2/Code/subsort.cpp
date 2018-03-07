#include <Windows.h>
#include <iostream>
#include <stdlib.h>

using namespace std;

#define BUF_SIZE (sizeof(double) << 20)
#define NUM_RANDOM_NUMBERS 1000000
wchar_t SMY_Name[20];				//共享内存名
wchar_t SEM_Name[20];				//用于控制进程数量的信号量名
HANDLE hMapFile = NULL;			//共享内存句柄
double* pBuf = NULL;			//共享内存地址
HANDLE hSemaphore = NULL;		//信号量句柄


//寻找划分元素
int partition(double *a, int left, int right) {
	int i = left - 1, j = right;
	double e = a[right];

	while (true) {
		while (a[++i] < e);
		while (e <= a[--j])
			if (j == left) break;
		if (i >= j) break;
		
		double t1 = a[i];
		a[i] = a[j];
		a[j] = t1;
	}

	double t2 = a[i];
	a[i] = a[right];
	a[right] = t2;

	return i;
}

void quick_sort(double *a, int left, int right) {
	if (right <= left) return;
	int pos = partition(a, left, right);
	quick_sort(a, left, pos - 1);
	quick_sort(a, pos + 1, right);
}

//多进程快速排序
void multproc_quick_sort(double *a, int left, int right, int min_part_num = 1000) {
	if (right - left <= min_part_num) { quick_sort(a, right, left); return; }
	else {
		DWORD dwWaitResult;
		wchar_t lpCommandLine1[50];
		wchar_t lpCommandLine2[50];
		STARTUPINFO si1;
		STARTUPINFO si2;
		PROCESS_INFORMATION pi1;
		PROCESS_INFORMATION pi2;

		int pos = partition(a, left, right);
		//分割后数据较少，直接排序; 否则产生新的进程
		if (pos - left <= min_part_num) quick_sort(a, left, pos - 1);
		else {
			//尝试获取信号量
			dwWaitResult = WaitForSingleObject(hSemaphore, 0L);
			
			//可以创建新的子进程
			if (dwWaitResult == WAIT_OBJECT_0) {
				wsprintf(lpCommandLine1, L"subsort.exe %s %s %d %d", SMY_Name, SEM_Name, left, pos - 1);
				//wcout << lpCommandLine1 << endl;
				ZeroMemory(&si1, sizeof(si1));
				si1.cb = sizeof(si1);
				ZeroMemory(&pi1, sizeof(pi1));

				if (!CreateProcess(NULL, lpCommandLine1, NULL, NULL, FALSE, 0, NULL, NULL, &si1, &pi1))
				{
					DWORD error_code = GetLastError();
					cout << "- 无法创建子进程, 错误码: " << error_code << " -" << '\n';
					UnmapViewOfFile(pBuf);
					CloseHandle(hMapFile);
					CloseHandle(hSemaphore);
					system("pause");
					exit(1);
				}
			}
			//无法创建
			else {
				multproc_quick_sort(a, left, pos - 1);
				multproc_quick_sort(a, pos + 1, right);
				return;
			}
		}

		//分割后数据较少，直接排序; 否则产生新的进程
		if (right - pos <= min_part_num) quick_sort(a, pos + 1, right);
		else {
			//尝试获取信号量
			dwWaitResult = WaitForSingleObject(hSemaphore, 0L);

			//可以创建新的子进程
			if (dwWaitResult == WAIT_OBJECT_0) {
				wsprintf(lpCommandLine2, L"subsort.exe %s %s %d %d", SMY_Name, SEM_Name, pos + 1, right);
				//wcout << lpCommandLine2 << endl;
				ZeroMemory(&si2, sizeof(si2));
				si2.cb = sizeof(si2);
				ZeroMemory(&pi2, sizeof(pi2));

				if (!CreateProcess(NULL, lpCommandLine2, NULL, NULL, FALSE, 0, NULL, NULL, &si2, &pi2))
				{
					DWORD error_code = GetLastError();
					cout << "- 无法创建子进程, 错误码: " << error_code << " -" << '\n';
					UnmapViewOfFile(pBuf);
					CloseHandle(hMapFile);
					CloseHandle(hSemaphore);
					system("pause");
					exit(1);
				}
			}
			//无法创建
			else {
				multproc_quick_sort(a, pos + 1, right);
				return;
			}
		}
	}
}


int wmain(int argc, wchar_t *argv[]) {
	if (argc != 5) {
		cout << "- 运行 subsort.exe 需5个参数 -" << '\n';
		return 1;
	}
	
	//获取共享内存名
	wcscpy(SMY_Name, argv[1]);
	//获取信号量名
	wcscpy(SEM_Name, argv[2]);
	//获得left参数
	int left = _wtoi(argv[3]);
	//获得right参数
	int right = _wtoi(argv[4]);


	//1.打开共享内存
	hMapFile = OpenFileMapping(
		FILE_MAP_ALL_ACCESS,				// read/write access
		FALSE,								// do not inherit the name
		SMY_Name);							// name of mapping object

	if (hMapFile == NULL)
	{
		DWORD error_code = GetLastError();
		cout << "- 无法打开文件映射对象, 错误码:" << error_code << " -" << '\n';
		system("pause");
		return 1;
	}
	
	pBuf = (double*)MapViewOfFile(hMapFile,			// handle to map object
		FILE_MAP_ALL_ACCESS,						// read/write permission
		0,
		0,
		BUF_SIZE);

	if (pBuf == NULL)
	{
		DWORD error_code = GetLastError();
		cout << "- 无法打开文件映射视图, 错误码: " << error_code << " -" << '\n';
		CloseHandle(hMapFile);
		system("pause");
		return 1;
	}

	
	//2.打开用于控制进程数的信号量
	hSemaphore = OpenSemaphore(SEMAPHORE_ALL_ACCESS, true, SEM_Name);
	if (hSemaphore == NULL)
	{
		DWORD error_code = GetLastError();
		cout << "- 无法打开信号量, 错误码: " << error_code << " -" << '\n';
		UnmapViewOfFile(pBuf);
		CloseHandle(hMapFile);
		system("pause");
		return 1;
	}


	//3.排序
	multproc_quick_sort(pBuf, left, right);


	//收尾
	//释放信号量
	if (!ReleaseSemaphore(hSemaphore, 1, NULL)) cout << "- 释放信号量错误, 错误码: " << GetLastError() << " -" << '\n';
	UnmapViewOfFile(pBuf);
	CloseHandle(hMapFile);
	CloseHandle(hSemaphore);
	return 0;
}