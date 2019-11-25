function newCounter()
  local i = 0
  return function() -- anonymous function
    i = i + 1
    return i
    end,function() -- anonymous function
    i = i + 10
    return i
    end
end
c1,c2 = newCounter()
print(c1()) --> 1
print(c2()) --> 2