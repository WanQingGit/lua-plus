tab  = {}
setmetatable(tab, {__mode = "kv"})  -- 设置weak table
key = {}  -- 对象1
tab[key] = 1
key = {}  -- 对象2
tab[key] = 2
collectgarbage()    -- 进行垃圾收集
for k,v in pairs(tab) do
  print(k,v)  --> table: 0059C300 2
end
