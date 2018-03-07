import multiprocessing as mp
from random import random, seed
from os.path import exists
from time import sleep, clock
import cProfile

#用于生成1000000个随机数
def generate_random_number():
	with open('./random_numbers.txt', 'w') as f:
		for i in range(1000):
			#更新随机数种子
			seed()
			for j in range(1000):
				num = ('%.10f' % random())
				f.write(num + ' ')
			f.write('\n')
	return

#读取随机数文件
def read_random_number():
	a = list()
	with open('./random_numbers.txt', 'r') as f:
		for line in f.readlines():
			for num in line.split():
				a.append(float(num))
	return a

#保存排序后随机数为文件
def save_sorted_random_number(a):
	with open('./sorted_numbers_python.txt', 'w') as f:
		for i in range(1000):
			for j in range(1000):
				num = ('%.10f' % a[1000*i + j])
				f.write(num + ' ')
			f.write('\n')
	return



#多进程快速排序
def partition(a, left, right):
	#寻找划分元素
	v = a[right]

	i, j = left, right
	while True:
		while (a[i] <= v) and (i < j):
			i += 1
		while (a[j] >= v) and (i < j):
			j -= 1
		if(i >= j): break
		#交换元素
		a[i], a[j] = a[j], a[i]
	a[i], a[right] = a[right], a[i]
	return i

			
#直接排序
def list_sort(a, left, right):
	b = [a[i] for i in range(left, right + 1)]
	b.sort()
	for i in range(right - left + 1):
		a[left + i] = b[i]
	return

#输入为随机数组
def quick_sort(a, left, right):
	print(right - left + 1)

	#如果长度过小，直接排序，不继续分割
	if(right - left <= 1000):
		list_sort(a, left, right)
		return

	#长度小于16000，所有进程并行操作
	elif(right - left <= 16000):
		pos = partition(a, left, right)
		#利用 共享内存 实现父进程与子进程间的通信
		cld_sort1 = mp.Process(target = quick_sort, args = (a, left, pos - 1))
		cld_sort2 = mp.Process(target = quick_sort, args = (a, pos + 1, right))
		cld_sort1.start()
		cld_sort2.start()
		cld_sort1.join()
		cld_sort2.join()
		return

	#长度过大，则只允许每个进程一次启动一个子进程，避免进程数过多
	else:
		pos = partition(a, left, right)
		#利用 共享内存 实现父进程与子进程间的通信
		cld_sort1 = mp.Process(target = quick_sort, args = (a, left, pos - 1))
		cld_sort1.start()
		cld_sort1.join()

		cld_sort2 = mp.Process(target = quick_sort, args = (a, pos + 1, right))
		cld_sort2.start()
		cld_sort2.join()
		return


def foo():
	#读取随机数文件
	if not exists('./random_number.txt'):
		generate_random_number()
	a = read_random_number()

	#a = list(range(50))
	#a.reverse()

	#利用 "共享内存" 实现父进程与子进程间的通信
	shared_a, left, right = mp.Array('d', a, lock = False), 0, (len(a) - 1)

	#排序
	main_sort = mp.Process(target = quick_sort, args = (shared_a, left, right))
	main_sort.start()
	main_sort.join()
	#print(shared_a[:])
	save_sorted_random_number([sorted_num for sorted_num in shared_a[:]])

if __name__ == '__main__':
	cProfile.run('foo()')

	


