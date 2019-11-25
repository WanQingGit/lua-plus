-- 记录开始时间
local starttime = os.clock()                           --> os.clock()用法
print(string.format("start time : %.4f", starttime))
local key={"one","two","three","four"}
-- 进行耗时操作
local sum = 0;
local b={}
for i = 1, 400000 do
  b={key=i}
  for j=1,#key do
    b[key[j]]=i+j
    sum =sum+i
  end
  b={}
  for j=1,#key do
    b[key[j]]=i
  end
end

-- 记录结束时间
local endtime = os.clock()                           --> os.clock()用法
print(string.format("end time   : %.4f,sum %d", endtime,sum))
print(string.format("cost time  : %.4f", endtime - starttime))
