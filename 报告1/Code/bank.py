import multiprocessing as mp
from time import sleep, clock


#顾客
def Client(client_id, enter_time, serve_time, get_mutex, client_queue):
	#print('client(' + str(client_id) + ')', 'enter bank.')

	#拿号并排队过程
	get_mutex.acquire()
	#管道，用于传递柜员序号
	(conn1, conn2) = mp.Pipe()
	wait_semaphore = mp.Manager().Semaphore(value = 0)
	finish_semaphore = mp.Manager().Semaphore(value = 0)
	client_queue.put((conn2, wait_semaphore, finish_semaphore))
	get_time = clock()
	get_mutex.release()

	#print('client(' + str(client_id) + ')', 'get number.')
	#顾客阻塞，直到等待被叫到号
	wait_semaphore.acquire()
	start_time = clock()

	#相当于进行一定时长的服务时间
	sleep(serve_time)
	counter_id = conn1.recv()
	end_time = clock()
	print('client %d:' % (client_id), enter_time + get_time, start_time + enter_time - get_time, end_time + enter_time - get_time, counter_id)
	finish_semaphore.release()
	#print('client(' + str(client_id) + ')', 'leave bank.')



#银行柜员
def Counter(counter_id, call_mutex, client_queue):
	while True:
		#获取锁，用于保证一个时刻只能有一个柜员叫号
		call_mutex.acquire()
		#叫号，没有则阻塞
		conn, wait_semaphore, finish_semaphore = client_queue.get()
		#叫到号，允许其他柜员叫号
		call_mutex.release()

		#print('an counter starts serving a client.')
		#进行服务. 并阻塞直至完成顾客需求
		conn.send(counter_id)
		wait_semaphore.release()
		finish_semaphore.acquire()
		

if __name__ == '__main__':

	counter_num = input('number of counter: ')
	counter_num = int(counter_num)

	#获取测试顾客信息
	clients_info = []
	with open('bank_test.txt') as f:
		for line in f.readlines():
			clients_info.append( tuple(int(x) for x in line.split()) )
	#根据进入时间排序
	clients_info.sort(key = lambda x: x[1])


	#顾客队列(存储顾客的锁)
	client_queue = mp.Queue()
	#号锁（二元信号量）
	get_mutex = mp.Lock()
	#叫号锁（二元信号量）
	call_mutex = mp.Lock()

	#银行柜员开始工作
	counters = []
	for i in range(counter_num):
		counter = mp.Process(target = Counter, args = (i + 1, call_mutex, client_queue))
		counter.start()
		#print('counter', str(i + 1), 'starts working.')
		counters.append(counter)


	#顾客开始进入银行
	setup_time = 0
	for client_id, enter_time, serve_time in clients_info:
		sleep(enter_time - setup_time)
		setup_time = enter_time

		client = mp.Process(target = Client, args = (client_id, enter_time, serve_time, get_mutex, client_queue))
		client.start()


	#主进程等待40秒后结束所有柜员进程
	sleep(40)
	for counter in counters:
		counter.terminate()

		
