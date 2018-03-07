NUM_CLIENT = 20;

% 利用指数分布累加模拟泊松分布
r = exprnd(5, [1, NUM_CLIENT]);
r = ceil(r);
enter_time = cumsum(r);

% 服务时间
serve_time = ceil(exprnd(5, [1, NUM_CLIENT]));

% 写入文件
f = fopen('./client_stream.txt', 'w');
for i = 1:numel(enter_time)
    fprintf(f, '%d %d %d\n', i, enter_time(i), serve_time(i));
end
fclose(f);