NUM_CLIENT = 20;

% ����ָ���ֲ��ۼ�ģ�Ⲵ�ɷֲ�
r = exprnd(5, [1, NUM_CLIENT]);
r = ceil(r);
enter_time = cumsum(r);

% ����ʱ��
serve_time = ceil(exprnd(5, [1, NUM_CLIENT]));

% д���ļ�
f = fopen('./client_stream.txt', 'w');
for i = 1:numel(enter_time)
    fprintf(f, '%d %d %d\n', i, enter_time(i), serve_time(i));
end
fclose(f);