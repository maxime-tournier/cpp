function add(x)
   return function(y)
	  return x + y
   end
end

function sub(x)
   return function(y)
	  return x - y
   end
end

function mul(x)
   return function(y)
	  return x * y
   end
end

function eq(x)
   return function(y)
	  return x == y
   end
end
