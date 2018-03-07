#include <Windows.h>
#include <iostream>
#include <stdlib.h>

using namespace std;

#define BUF_SIZE (sizeof(double) << 20)
#define NUM_RANDOM_NUMBERS 1000000
wchar_t SMY_Name[20];				//�����ڴ���
wchar_t SEM_Name[20];				//���ڿ��ƽ����������ź�����
HANDLE hMapFile = NULL;			//�����ڴ���
double* pBuf = NULL;			//�����ڴ��ַ
HANDLE hSemaphore = NULL;		//�ź������


//Ѱ�һ���Ԫ��
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

//����̿�������
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
		//�ָ�����ݽ��٣�ֱ������; ��������µĽ���
		if (pos - left <= min_part_num) quick_sort(a, left, pos - 1);
		else {
			//���Ի�ȡ�ź���
			dwWaitResult = WaitForSingleObject(hSemaphore, 0L);
			
			//���Դ����µ��ӽ���
			if (dwWaitResult == WAIT_OBJECT_0) {
				wsprintf(lpCommandLine1, L"subsort.exe %s %s %d %d", SMY_Name, SEM_Name, left, pos - 1);
				//wcout << lpCommandLine1 << endl;
				ZeroMemory(&si1, sizeof(si1));
				si1.cb = sizeof(si1);
				ZeroMemory(&pi1, sizeof(pi1));

				if (!CreateProcess(NULL, lpCommandLine1, NULL, NULL, FALSE, 0, NULL, NULL, &si1, &pi1))
				{
					DWORD error_code = GetLastError();
					cout << "- �޷������ӽ���, ������: " << error_code << " -" << '\n';
					UnmapViewOfFile(pBuf);
					CloseHandle(hMapFile);
					CloseHandle(hSemaphore);
					system("pause");
					exit(1);
				}
			}
			//�޷�����
			else {
				multproc_quick_sort(a, left, pos - 1);
				multproc_quick_sort(a, pos + 1, right);
				return;
			}
		}

		//�ָ�����ݽ��٣�ֱ������; ��������µĽ���
		if (right - pos <= min_part_num) quick_sort(a, pos + 1, right);
		else {
			//���Ի�ȡ�ź���
			dwWaitResult = WaitForSingleObject(hSemaphore, 0L);

			//���Դ����µ��ӽ���
			if (dwWaitResult == WAIT_OBJECT_0) {
				wsprintf(lpCommandLine2, L"subsort.exe %s %s %d %d", SMY_Name, SEM_Name, pos + 1, right);
				//wcout << lpCommandLine2 << endl;
				ZeroMemory(&si2, sizeof(si2));
				si2.cb = sizeof(si2);
				ZeroMemory(&pi2, sizeof(pi2));

				if (!CreateProcess(NULL, lpCommandLine2, NULL, NULL, FALSE, 0, NULL, NULL, &si2, &pi2))
				{
					DWORD error_code = GetLastError();
					cout << "- �޷������ӽ���, ������: " << error_code << " -" << '\n';
					UnmapViewOfFile(pBuf);
					CloseHandle(hMapFile);
					CloseHandle(hSemaphore);
					system("pause");
					exit(1);
				}
			}
			//�޷�����
			else {
				multproc_quick_sort(a, pos + 1, right);
				return;
			}
		}
	}
}


int wmain(int argc, wchar_t *argv[]) {
	if (argc != 5) {
		cout << "- ���� subsort.exe ��5������ -" << '\n';
		return 1;
	}
	
	//��ȡ�����ڴ���
	wcscpy(SMY_Name, argv[1]);
	//��ȡ�ź�����
	wcscpy(SEM_Name, argv[2]);
	//���left����
	int left = _wtoi(argv[3]);
	//���right����
	int right = _wtoi(argv[4]);


	//1.�򿪹����ڴ�
	hMapFile = OpenFileMapping(
		FILE_MAP_ALL_ACCESS,				// read/write access
		FALSE,								// do not inherit the name
		SMY_Name);							// name of mapping object

	if (hMapFile == NULL)
	{
		DWORD error_code = GetLastError();
		cout << "- �޷����ļ�ӳ�����, ������:" << error_code << " -" << '\n';
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
		cout << "- �޷����ļ�ӳ����ͼ, ������: " << error_code << " -" << '\n';
		CloseHandle(hMapFile);
		system("pause");
		return 1;
	}

	
	//2.�����ڿ��ƽ��������ź���
	hSemaphore = OpenSemaphore(SEMAPHORE_ALL_ACCESS, true, SEM_Name);
	if (hSemaphore == NULL)
	{
		DWORD error_code = GetLastError();
		cout << "- �޷����ź���, ������: " << error_code << " -" << '\n';
		UnmapViewOfFile(pBuf);
		CloseHandle(hMapFile);
		system("pause");
		return 1;
	}


	//3.����
	multproc_quick_sort(pBuf, left, right);


	//��β
	//�ͷ��ź���
	if (!ReleaseSemaphore(hSemaphore, 1, NULL)) cout << "- �ͷ��ź�������, ������: " << GetLastError() << " -" << '\n';
	UnmapViewOfFile(pBuf);
	CloseHandle(hMapFile);
	CloseHandle(hSemaphore);
	return 0;
}