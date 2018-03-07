#include <Windows.h>
#include <tchar.h>
#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <time.h>


using namespace std;

#define BUF_SIZE (sizeof(double) << 20)
#define NUM_RANDOM_NUMBERS 1000000			//���������
#define MAX_NUM_PROC 25						//��������

//��ȡ������ļ�
bool read_random_number(string file_pth, double *random_numbers) {
	ifstream in;
	in.open(file_pth, ios::in);
	if (!in.is_open()) {
		cout << "- �޷���������ļ� -" << '\n';
		return false;
	}

	//������ļ�Ϊ 1000�� * 1000����/ÿ�У�ÿ��������Ϊ12���ַ����ַ�������"0.7790856164"
	char buffer[15000];
	cout << "- ��ȡ������� -" << '\n';
	for(int i = 0; i < 1000; i++)
	{
		//��ȡһ��
		in.getline(buffer, 15000, '\n');
		
		//ת��Ϊdouble��
		char str_number[13];
		for (int j = 0; j < 1000; j++) {
			for (int k = 0; k < 12; k++) {
				str_number[k] = buffer[j * 13 + k];
				sscanf(str_number, "%12lf", &(random_numbers[1000 * i + j]));
			}
		}
	}

	cout << "- ��ȡ������ļ���� -" << '\n';
	in.close();
	return true;
}

void save_sorted_random_number(string file_pth, double *random_numbers) {
	ofstream out;
	out.open(file_pth, ios::out);

	cout << "- д��������������� -" << '\n';
	char buffer[13];
	for (int i = 0; i < 1000; i++) {
		for (int j = 0; j < 1000; j++) {
			sprintf(buffer, "%12lf", random_numbers[1000 * i + j]);
			out << buffer << ' ';
		}
		out << '\n';
	}
	out.close();

	cout << "- д���������������� -" << '\n';
}

int main() {
	//1.���������ڴ�����
	TCHAR hMapName[] = TEXT("NumberRegion");
	HANDLE hMapFile = CreateFileMapping(
		INVALID_HANDLE_VALUE,				//�����ڴ�
		NULL,
		PAGE_READWRITE,						//�ɶ���д
		0,						
		BUF_SIZE,							//BUF_SIZE MB				
		hMapName);

	if (hMapFile == NULL)
	{
		DWORD error_code = GetLastError();
		cout << "- �޷������ļ�ӳ�����, ������: " << error_code << " -" << '\n';
		system("pause");
		return 1;
	}
	cout << "- �����ļ�ӳ�������� -" << '\n';


	//2.�ڴ�ӳ��
	double* pBuf = (double*)MapViewOfFile(hMapFile,   
		FILE_MAP_ALL_ACCESS,				//�ɶ���д
		0,
		0,
		BUF_SIZE);							//BUF_SIZE MB

	if (pBuf == NULL)
	{
		DWORD error_code = GetLastError();
		cout << "- �޷������ڴ�ӳ���ļ�, ������: " << error_code << " -" << '\n';
		CloseHandle(hMapFile);
		system("pause");
		return 1;
	}
	cout << "- �����ڴ�ӳ���ļ���� -" << '\n';


	//*****************************
	//*ʹ��ʱ�������޸�·����лл~*
	//*****************************
	double *random_numbers = new double[NUM_RANDOM_NUMBERS];
	if (!read_random_number(".\\random_numbers.txt", random_numbers)) {
		UnmapViewOfFile(pBuf);
		CloseHandle(hMapFile);
		system("pause");
		return 1;
	}
	
	//3.�����鸴�ƽ������ڴ�
	CopyMemory((PVOID)pBuf, random_numbers, sizeof(double) * NUM_RANDOM_NUMBERS);


	//4.�������ڿ��ƽ�����Ŀ���ź���
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
		cout << "- �޷������ź���, ������: " << error_code << " -" << '\n';
		delete[] random_numbers;
		UnmapViewOfFile(pBuf);
		CloseHandle(hMapFile);
		system("pause");
		return 1;
	}


	//5.�����ӽ���
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
		cout << "- �޷������ӽ���, ������: " << error_code << " -" << '\n';
		delete[] random_numbers;
		CloseHandle(hSemaphore);
		UnmapViewOfFile(pBuf);
		CloseHandle(hMapFile);
		system("pause");
		return 1;
	}

	cout << "- ����̿�������ʼ -" << '\n';

	//�ȴ������ӽ��̽���
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
	cout << "- ����̿����������, ��ʱ: " << (double)(finish - start)/CLOCKS_PER_SEC << "s -" << '\n';


	//�������������д���ļ�
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