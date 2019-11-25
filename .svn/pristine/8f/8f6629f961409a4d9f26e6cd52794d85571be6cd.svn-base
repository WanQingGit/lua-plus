--[ 定义变量 --]
a = 14

--[ while 循环 --]
while( a < 20 )
do
  print("a 的值为:", a)
  a=a+1
  if( a > 15)
  then
    --[ 使用 break 语句终止循环 --]
    break
  end
end

--0 OP_SETTABUP: A 0,B 256,C 257
--1 OP_GETTABUP: A 0,B 0,C 256
--2 OP_LT: A 1,B 0,C 258
--3 OP_JMP: A 0,B -1
--4 OP_GETTABUP: A 0,B 0,C 259
--5 OP_LOADK: A 1,Bx 4
--6 OP_GETTABUP: A 0,B 0,C 256
--7 OP_CALL: A 0,B 3,C 2
--8 OP_GETTABUP: A 0,B 0,C 256
--9 OP_ADD: A 0,B 0,C 261
--10 OP_SETTABUP: A 0,B 256,C 0
--11 OP_GETTABUP: A 0,B 0,C 256
--12 OP_LT: A 1,B 262,C 0
--13 OP_JMP: A 0,B -1
--14 OP_JMP: A 0,B -1
--fixjump 14 1
--fixjump 13 3
--fixjump 13 15
--fixjump 3 15
--15 OP_RETURN: A 0,B 1,C 0
--0 OP_SETTABUP: A 0,B 256,C 257
--1 OP_GETTABUP: A 0,B 0,C 256
--2 OP_LT: A 0,B 0,C 258
--3 OP_GETTABUP: A 0,B 0,C 259
--4 OP_LOADK: A 1,Bx 4
--5 OP_GETTABUP: A 2,B 0,C 256
--6 OP_CALL: A 0,B 3,C 1
--a 的值为:  14
--7 OP_GETTABUP: A 0,B 0,C 256
--8 OP_ADD: A 0,B 0,C 261
--9 OP_SETTABUP: A 0,B 256,C 0
--10 OP_GETTABUP: A 0,B 0,C 256
--11 OP_LT: A 1,B 262,C 0
--12 OP_JMP: A 0,B -14
--dojump: A 0,B -14
--13 OP_GETTABUP: A 0,B 0,C 256
--14 OP_LT: A 0,B 0,C 258
--15 OP_GETTABUP: A 0,B 0,C 259
--16 OP_LOADK: A 1,Bx 4
--17 OP_GETTABUP: A 2,B 0,C 256
--18 OP_CALL: A 0,B 3,C 1
--a 的值为:  15
--19 OP_GETTABUP: A 0,B 0,C 256
--20 OP_ADD: A 0,B 0,C 261
--21 OP_SETTABUP: A 0,B 256,C 0
--22 OP_GETTABUP: A 0,B 0,C 256
--23 OP_LT: A 1,B 262,C 0
--dojump: A 0,B 1
--24 OP_RETURN: A 0,B 1,C 0