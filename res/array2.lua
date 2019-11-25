name = {"you" ,"me", "him","bill" }
--table.sort - only works with arrays!
table.sort(name)
for k, v in ipairs(name) do
     print( k,v)
end