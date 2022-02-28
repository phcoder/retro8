const char* lua_api_string =
"function all(t)\n"
"  if t ~= nil then\n"
"    local nt = {}\n"
"    local ni = 1\n"
"    for _,v in pairs(t) do\n"
"      nt[ni] = v\n"
"      ni = ni + 1\n"
"    end\n"
"    for k,v in pairs(nt) do\n"
"    end\n"
"\n"
"    local i = 0\n"
"    return function() i = i + 1; return nt[i] end\n"
"  end\n"
"  return function() end\n"
"end\n"
"\n"
"function add(t, v)\n"
"  if t ~= nil then\n"
"    t[#t+1] = v\n"
"    return v\n"
"  end\n"
"end\n"
"\n"
"function foreach(c, f)\n"
"  if c ~= nil then\n"
"    for value in all(c) do\n"
"      f(value)\n"
"    end\n"
"  end\n"
"end\n"
"\n"
"\n"
"function mapdraw(...)\n"
"  map(table.unpack(arg))\n"
"end\n"
"\n"
"function count(t)\n"
"  if t ~= nil then\n"
"    return #t\n"
"  end\n"
"end\n"
"\n"
"function del(t, v)\n"
"  if t ~= nil then\n"
"    local found = false\n"
"    for i = 1, #t do\n"
"      if t[i] == v then\n"
"        found = true\n"
"      end\n"
"      if found then\n"
"        t[i] = t[i+1]\n"
"      end\n"
"    end\n"
"    if found then\n"
"      return v\n"
"    end\n"
"  end\n"
"end\n"
"\n"
"function cocreate(f)\n"
"  return coroutine.create(f)\n"
"end\n"
"\n"
"function yield()\n"
"  coroutine.yield()\n"
"end\n"
"\n"
"-- TODO: missing vararg\n"
"function coresume(f)\n"
"  return coroutine.resume(f)\n"
"end\n"
"\n"
"function costatus(f)\n"
"  return coroutine.status(f)\n"
"end";
