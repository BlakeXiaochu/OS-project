#include <Windows.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <fstream>
#include <time.h>

using namespace std;

//用于记录顾客信息
struct client_info
{
	int id;				//序号
	int enter_time;		//进入时间
	int serve_time;		//服务时间

	client_info() {
		this->id = 0;
		this->enter_time = 0;
		this->serve_time = 0;
	};
};

//用于顾客队列的数据结构
struct client_queue_elem
{
	client_info cinfo;				//用于该柜员和顾客交流的管道
	HANDLE wait_event;				//用于触发顾客被叫到号事件
	HANDLE finish_event;			//用于触发顾客结束服务事件
};

#define NUM_COUNTER 5				//最大柜员数
HANDLE get_mutex;					//拿号锁
HANDLE call_mutex;					//叫号锁

vector<client_queue_elem> client_queue;				//顾客队列
CRITICAL_SECTION CriticalSection;					//顾客队列临界区
vector<int> counter_client_pair;					//用于保存每队柜员-顾客信息


//读取顾客信息文件
bool read_client_info(vector<client_info> &a, string path = ".\\bank_test.txt") {
	ifstream in;
	in.open(path, ios::in);
	if (!in.is_open()) {
		cout << "- 读取顾客信息文件失败 -" << '\n';
		return false;
	}

	char buffer[100];
	while (!in.eof()) {
		for (int i = 0; i < 99; i++) buffer[i] = ' ';

		in.getline(buffer, 100, '\n');

		int j = 0;
		client_info b;
		b.id = atoi(buffer);

		while (buffer[j++] != ' ');
		b.enter_time = atoi(&(buffer[j]));

		while (buffer[j++] != ' ');
		b.serve_time = atoi(&(buffer[j]));

		if ((b.id == 0) && (b.enter_time == 0) && (b.serve_time == 0)) continue;
		a.push_back(b);
	}

	in.close();
	return true;
}

//用于对顾客进入时间排序
bool compare(const client_info &a, const client_info&b) {
	if (a.enter_time < b.enter_time) return 1;
	else return 0;
}


//银行柜员
DWORD WINAPI Counter(LPVOID lpParam) {
	int counter_id = *((int*)lpParam);

	EnterCriticalSection(&CriticalSection);
	cout << "- 柜员" << counter_id << "开始工作 -" << '\n';
	LeaveCriticalSection(&CriticalSection);

	client_info cinfo;
	HANDLE wait_event;
	HANDLE finish_event;
	while (true) {
		//保证一个时刻只能有一个柜员叫号
		DWORD callWaitResult = WaitForSingleObject(call_mutex, INFINITE);

		//成功叫号
		if (callWaitResult == WAIT_OBJECT_0) {
			//进入临界区
			EnterCriticalSection(&CriticalSection);

			//若无新顾客，柜员等待, 并退出临界区
			if (client_queue.empty()) {
				LeaveCriticalSection(&CriticalSection);
				ReleaseMutex(call_mutex);
				Sleep(50);
				continue; 
			}

			client_queue_elem a;
			a = client_queue.back();

			cinfo = a.cinfo;
			wait_event = a.wait_event;
			finish_event = a.finish_event;
			client_queue.pop_back();
			counter_client_pair[counter_id] = a.cinfo.id;

			//退出临界区
			LeaveCriticalSection(&CriticalSection);
			//叫到号，允许其他柜员叫号
			ReleaseMutex(call_mutex);
		}
		else continue;

		//进行服务.并阻塞直至完成顾客需求
		SetEvent(wait_event);
		WaitForSingleObject(finish_event, INFINITE);
		counter_client_pair[counter_id] = -1;
	}
	return 0;
}


//顾客
DWORD WINAPI Client(LPVOID lpParam) {
	client_info cinfo = *((client_info*)lpParam);

	cout << "- 顾客" << cinfo.id << "进入银行 -" << '\n';

	time_t get_time;	//拿到号的时间
	time_t start_time;	//开始服务的时间
	time_t end_time;	//结束时间

	HANDLE wait_event;		//用于触发顾客被叫到号事件
	HANDLE finish_event;	//用于触发顾客结束服务事件

	//保证一个时刻只能有一个顾客拿号
	DWORD getWaitResult = WaitForSingleObject(get_mutex, INFINITE);

	//拿到号并排队过程
	if (getWaitResult == WAIT_OBJECT_0) {
		//进入临界区
		EnterCriticalSection(&CriticalSection);

		wait_event = CreateEvent(NULL, TRUE, FALSE, NULL);
		finish_event = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (wait_event == NULL){
			cout << "- 无法创建等待事件, 错误码: " << GetLastError() << " -" << '\n';
			LeaveCriticalSection(&CriticalSection);
			ReleaseMutex(get_mutex);
			system("pause");
			return 1;
		}
		if (finish_event == NULL) {
			cout << "- 无法创建完成事件, 错误码: " << GetLastError() << " -" << '\n';
			LeaveCriticalSection(&CriticalSection);
			ReleaseMutex(get_mutex);
			CloseHandle(wait_event);
			system("pause");
			return 1;
		}

		client_queue_elem a;
		a.cinfo = cinfo;
		a.wait_event = wait_event;
		a.finish_event = finish_event;

		//将顾客信息保存进顾客队列
		client_queue.insert(client_queue.begin(), a);
		get_time = time(NULL);

		//退出临界区
		LeaveCriticalSection(&CriticalSection);
		ReleaseMutex(get_mutex);		
	}
	else return 1;

	//顾客阻塞，直到等待被叫到号
	WaitForSingleObject(wait_event, INFINITE);
	start_time = time(NULL);

	//获取柜员号
	int client_id = -1;
	for (int j = 0; j < (int)counter_client_pair.size(); j++) {
		if (counter_client_pair[j] == cinfo.id) { client_id = j; break; }
	}

	//用sleep模拟进行一段时间的服务
	Sleep(cinfo.serve_time * 1000);

	end_time = time(NULL);
	cout << "顾客" << cinfo.id << ": " << cinfo.enter_time << ' ' << start_time + cinfo.enter_time - get_time 
		 << ' ' << end_time + cinfo.enter_time - get_time << ' ' << client_id << '\n';

	//通知柜员: 顾客已完成服务
	SetEvent(finish_event);
	CloseHandle(wait_event);
	CloseHandle(finish_event);
	return 0;
}

int main() {
	//读取顾客信息
	vector<client_info> cinfo;
	string file_path = ".\\client_stream.txt";
	if (!read_client_info(cinfo, file_path)) return 1;

	//根据进入时间排序
	sort(cinfo.begin(), cinfo.end(), compare);

	//拿号锁（二元信号量）
	get_mutex = CreateMutex(NULL, FALSE, NULL);
	//叫号锁（二元信号量）
	call_mutex = CreateMutex(NULL, FALSE, NULL);
	if (get_mutex == NULL)
	{
		cout << "- 无法创建拿号锁, 错误码: " << GetLastError() << " -" << '\n';
		system("pause");
		return 1;
	}
	if (call_mutex == NULL)
	{
		cout << "- 无法创建叫号锁, 错误码: " << GetLastError() << " -" << '\n';
		CloseHandle(call_mutex);
		system("pause");
		return 1;
	}

	
	//利用全局队列变量保存顾客, 该队列只能在 临界区 中被操作
	if (!InitializeCriticalSectionAndSpinCount(&CriticalSection, 0x00000400)) {
		cout << "- 无法创临界区, 错误码: " << GetLastError() << " -" << '\n';
		CloseHandle(get_mutex);
		CloseHandle(call_mutex);
		system("pause");
		return 1;
	}

	//创建柜员子进程
	int* counter_id[NUM_COUNTER];
	DWORD counter_thread_id_array[NUM_COUNTER];
	HANDLE counter_thread_array[NUM_COUNTER];

	for (int i = 0; i < NUM_COUNTER; i++) {
		counter_id[i] = (int*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(int));
		*counter_id[i] = i;

		counter_thread_array[i] = CreateThread(NULL, 0, Counter, counter_id[i], 0, &counter_thread_id_array[i]);
		if (counter_thread_array[i] == NULL) {
			cout << "- 无法创建柜员进程, 错误码: " << GetLastError() << " -" << '\n';
			DeleteCriticalSection(&CriticalSection);
			CloseHandle(get_mutex);
			CloseHandle(call_mutex);
			system("pause");
			ExitProcess(3);
		}

		counter_client_pair.push_back(-1);
	}

	//顾客开始进入银行
	int setup_time = 0;
	const int NUM_CLIENT = cinfo.size();
	client_info** client_param = new client_info*[NUM_CLIENT];
	DWORD *client_thread_id_array = new DWORD[NUM_CLIENT];
	HANDLE *client_thread_array = new HANDLE[NUM_CLIENT];
	for (int i = 0; i < (int)cinfo.size(); i++) {
		Sleep(1000 * (cinfo[i].enter_time - setup_time));
		setup_time = cinfo[i].enter_time;

		client_param[i] = (client_info*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(client_info));
		client_param[i]->id = cinfo[i].id;
		client_param[i]->enter_time = cinfo[i].enter_time;
		client_param[i]->serve_time = cinfo[i].serve_time;

		client_thread_array[i] = CreateThread(NULL, 0, Client, client_param[i], 0, &client_thread_id_array[i]);
		if (client_thread_array[i] == NULL) {
			cout << "- 无法创建顾客进程, 错误码: " << GetLastError() << " -" << '\n';
			DeleteCriticalSection(&CriticalSection);
			CloseHandle(get_mutex);
			CloseHandle(call_mutex);
			system("pause");
			ExitProcess(3);
		}
	}

	//等待所有顾客完成服务
	WaitForMultipleObjects(NUM_CLIENT, client_thread_array, TRUE, INFINITE);

	//释放资源
	for (int i = 0; i < NUM_COUNTER; i++)
	{
		DWORD exit_code = 0;
		TerminateThread(counter_thread_array[i], exit_code);
		CloseHandle(counter_thread_array[i]);
		if (counter_id[i] != NULL)
		{
			HeapFree(GetProcessHeap(), 0, counter_id[i]);
			counter_id[i] = NULL;
		}
	}
	for (int i = 0; i < NUM_CLIENT; i++)
	{
		CloseHandle(client_thread_array[i]);
		if (client_param[i] != NULL)
		{
			HeapFree(GetProcessHeap(), 0, client_param[i]);
			client_param[i] = NULL;
		}
	}
	DeleteCriticalSection(&CriticalSection);
	CloseHandle(get_mutex);
	CloseHandle(call_mutex);
	system("pause");
	return 0;
}