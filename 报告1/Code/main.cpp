#include <Windows.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <fstream>
#include <time.h>

using namespace std;

//���ڼ�¼�˿���Ϣ
struct client_info
{
	int id;				//���
	int enter_time;		//����ʱ��
	int serve_time;		//����ʱ��

	client_info() {
		this->id = 0;
		this->enter_time = 0;
		this->serve_time = 0;
	};
};

//���ڹ˿Ͷ��е����ݽṹ
struct client_queue_elem
{
	client_info cinfo;				//���ڸù�Ա�͹˿ͽ����Ĺܵ�
	HANDLE wait_event;				//���ڴ����˿ͱ��е����¼�
	HANDLE finish_event;			//���ڴ����˿ͽ��������¼�
};

#define NUM_COUNTER 5				//����Ա��
HANDLE get_mutex;					//�ú���
HANDLE call_mutex;					//�к���

vector<client_queue_elem> client_queue;				//�˿Ͷ���
CRITICAL_SECTION CriticalSection;					//�˿Ͷ����ٽ���
vector<int> counter_client_pair;					//���ڱ���ÿ�ӹ�Ա-�˿���Ϣ


//��ȡ�˿���Ϣ�ļ�
bool read_client_info(vector<client_info> &a, string path = ".\\bank_test.txt") {
	ifstream in;
	in.open(path, ios::in);
	if (!in.is_open()) {
		cout << "- ��ȡ�˿���Ϣ�ļ�ʧ�� -" << '\n';
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

//���ڶԹ˿ͽ���ʱ������
bool compare(const client_info &a, const client_info&b) {
	if (a.enter_time < b.enter_time) return 1;
	else return 0;
}


//���й�Ա
DWORD WINAPI Counter(LPVOID lpParam) {
	int counter_id = *((int*)lpParam);

	EnterCriticalSection(&CriticalSection);
	cout << "- ��Ա" << counter_id << "��ʼ���� -" << '\n';
	LeaveCriticalSection(&CriticalSection);

	client_info cinfo;
	HANDLE wait_event;
	HANDLE finish_event;
	while (true) {
		//��֤һ��ʱ��ֻ����һ����Ա�к�
		DWORD callWaitResult = WaitForSingleObject(call_mutex, INFINITE);

		//�ɹ��к�
		if (callWaitResult == WAIT_OBJECT_0) {
			//�����ٽ���
			EnterCriticalSection(&CriticalSection);

			//�����¹˿ͣ���Ա�ȴ�, ���˳��ٽ���
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

			//�˳��ٽ���
			LeaveCriticalSection(&CriticalSection);
			//�е��ţ�����������Ա�к�
			ReleaseMutex(call_mutex);
		}
		else continue;

		//���з���.������ֱ����ɹ˿�����
		SetEvent(wait_event);
		WaitForSingleObject(finish_event, INFINITE);
		counter_client_pair[counter_id] = -1;
	}
	return 0;
}


//�˿�
DWORD WINAPI Client(LPVOID lpParam) {
	client_info cinfo = *((client_info*)lpParam);

	cout << "- �˿�" << cinfo.id << "�������� -" << '\n';

	time_t get_time;	//�õ��ŵ�ʱ��
	time_t start_time;	//��ʼ�����ʱ��
	time_t end_time;	//����ʱ��

	HANDLE wait_event;		//���ڴ����˿ͱ��е����¼�
	HANDLE finish_event;	//���ڴ����˿ͽ��������¼�

	//��֤һ��ʱ��ֻ����һ���˿��ú�
	DWORD getWaitResult = WaitForSingleObject(get_mutex, INFINITE);

	//�õ��Ų��Ŷӹ���
	if (getWaitResult == WAIT_OBJECT_0) {
		//�����ٽ���
		EnterCriticalSection(&CriticalSection);

		wait_event = CreateEvent(NULL, TRUE, FALSE, NULL);
		finish_event = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (wait_event == NULL){
			cout << "- �޷������ȴ��¼�, ������: " << GetLastError() << " -" << '\n';
			LeaveCriticalSection(&CriticalSection);
			ReleaseMutex(get_mutex);
			system("pause");
			return 1;
		}
		if (finish_event == NULL) {
			cout << "- �޷���������¼�, ������: " << GetLastError() << " -" << '\n';
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

		//���˿���Ϣ������˿Ͷ���
		client_queue.insert(client_queue.begin(), a);
		get_time = time(NULL);

		//�˳��ٽ���
		LeaveCriticalSection(&CriticalSection);
		ReleaseMutex(get_mutex);		
	}
	else return 1;

	//�˿�������ֱ���ȴ����е���
	WaitForSingleObject(wait_event, INFINITE);
	start_time = time(NULL);

	//��ȡ��Ա��
	int client_id = -1;
	for (int j = 0; j < (int)counter_client_pair.size(); j++) {
		if (counter_client_pair[j] == cinfo.id) { client_id = j; break; }
	}

	//��sleepģ�����һ��ʱ��ķ���
	Sleep(cinfo.serve_time * 1000);

	end_time = time(NULL);
	cout << "�˿�" << cinfo.id << ": " << cinfo.enter_time << ' ' << start_time + cinfo.enter_time - get_time 
		 << ' ' << end_time + cinfo.enter_time - get_time << ' ' << client_id << '\n';

	//֪ͨ��Ա: �˿�����ɷ���
	SetEvent(finish_event);
	CloseHandle(wait_event);
	CloseHandle(finish_event);
	return 0;
}

int main() {
	//��ȡ�˿���Ϣ
	vector<client_info> cinfo;
	string file_path = ".\\client_stream.txt";
	if (!read_client_info(cinfo, file_path)) return 1;

	//���ݽ���ʱ������
	sort(cinfo.begin(), cinfo.end(), compare);

	//�ú�������Ԫ�ź�����
	get_mutex = CreateMutex(NULL, FALSE, NULL);
	//�к�������Ԫ�ź�����
	call_mutex = CreateMutex(NULL, FALSE, NULL);
	if (get_mutex == NULL)
	{
		cout << "- �޷������ú���, ������: " << GetLastError() << " -" << '\n';
		system("pause");
		return 1;
	}
	if (call_mutex == NULL)
	{
		cout << "- �޷������к���, ������: " << GetLastError() << " -" << '\n';
		CloseHandle(call_mutex);
		system("pause");
		return 1;
	}

	
	//����ȫ�ֶ��б�������˿�, �ö���ֻ���� �ٽ��� �б�����
	if (!InitializeCriticalSectionAndSpinCount(&CriticalSection, 0x00000400)) {
		cout << "- �޷����ٽ���, ������: " << GetLastError() << " -" << '\n';
		CloseHandle(get_mutex);
		CloseHandle(call_mutex);
		system("pause");
		return 1;
	}

	//������Ա�ӽ���
	int* counter_id[NUM_COUNTER];
	DWORD counter_thread_id_array[NUM_COUNTER];
	HANDLE counter_thread_array[NUM_COUNTER];

	for (int i = 0; i < NUM_COUNTER; i++) {
		counter_id[i] = (int*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(int));
		*counter_id[i] = i;

		counter_thread_array[i] = CreateThread(NULL, 0, Counter, counter_id[i], 0, &counter_thread_id_array[i]);
		if (counter_thread_array[i] == NULL) {
			cout << "- �޷�������Ա����, ������: " << GetLastError() << " -" << '\n';
			DeleteCriticalSection(&CriticalSection);
			CloseHandle(get_mutex);
			CloseHandle(call_mutex);
			system("pause");
			ExitProcess(3);
		}

		counter_client_pair.push_back(-1);
	}

	//�˿Ϳ�ʼ��������
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
			cout << "- �޷������˿ͽ���, ������: " << GetLastError() << " -" << '\n';
			DeleteCriticalSection(&CriticalSection);
			CloseHandle(get_mutex);
			CloseHandle(call_mutex);
			system("pause");
			ExitProcess(3);
		}
	}

	//�ȴ����й˿���ɷ���
	WaitForMultipleObjects(NUM_CLIENT, client_thread_array, TRUE, INFINITE);

	//�ͷ���Դ
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