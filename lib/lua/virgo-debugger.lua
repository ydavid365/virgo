--[[

Copyright 2012 The Luvit Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS-IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

This debugger was heavily inspired by Dave Nichols's debugger
which itself was inspired by:
RemDebug 1.0 Beta  (under Lua License)
Copyright Kepler Project 2005 (http://www.keplerproject.org/remdebug)
--]]

local debug = require('debug')
local string = require('string')
local table = require('table')
local utils = require('utils')
local Object = require('core').Object
local fmt = string.format

-- singleton debugger instance
local debugger = nil

-- all callable debugger commands
local commands = {}

-- internal response returned by a comand
local OPS = {
  -- continue normal execution
  ['c']   = 0,
  -- quit debugger
  ['q']   = 1,
  -- noop (ie, get more input)
  ['nop'] = 2
}

local STATE_DEFAULT = 0
local STATE_OVER    = 1
local STATE_OUT     = 2
local STATE_IN      = 4

local STATES = {
  [STATE_DEFAULT] = "DEFAULT",
  [STATE_OVER]    = "OVER",
  [STATE_OUT]     = "OUT",
  [STATE_IN]      = "IN"
}

local Command = Object:extend()

-- max short help text length (for formatting help strings)
local max_short_length = 0

function Command:initialize(name, short, long, f)
  self.name = name
  self.short = short
  self.long = long
  self.f = f
end

function Command:call(...)
  return self.f(...)
end

function Command:help()
  local num_spaces = max_short_length - #self.short
  return string.format("%s\t%s%s\t%s\n\n", self.name, self.short, string.rep(' ', num_spaces), self.long or ' ')
end

function make_command(name, short, ...)
  if short and #short > max_short_length then
    max_short_length = #short
  end

  assert(commands[name] == nil, "don't redifine command " .. name)
  commands[name] = Command:new(name, short, ...)
end

local Debugger = Object:extend()

local function getinfo(lvl)
  local info = debug.getinfo(lvl+1)
  if not info then
    return {nil, nil}
  end
  return {info.short_src, info.currentline}
end

local debugger_entry = function()
  local file, line = unpack(getinfo(2))
  debugger:set_breakpoint(file, line)
end

-- returns debugger's stack depth and the stack depth above that
local function get_lvl(lvl)
  local info
  local last_seen = 1
  lvl = lvl or 1

  while true do
    info = debug.getinfo(lvl, 'nfS')

    if not info then
      break
    -- oh god, this is terrible
    -- TODO: change to look for the entry points (ie, the functions, not the module)
    -- NOTE- different entry points and maybe stack trampolining sometimes makes this hard :(
    elseif info.short_src == '[string "modules/virgo-debugger.lua"]' then
      last_seen = lvl
    end
    lvl = lvl + 1
  end

  return {last_seen - 1, lvl - last_seen - 1}
end

local function getvalue(level, name)
  -- this is an efficient lookup of a name
  -- useful for potentially setting upvalues too
  local value, found, attrs

  attrs = name:split('%.')
  name = attrs[1]

  local function resolve_attrs()
    for i=2,#attrs do
      if not value or type(value) ~= "table" then
        p(value)
        break
      end
      value = value[attrs[i]]
    end

    return value
  end

  -- try local variables
  local i = 1
  while true do
    local n, v = debug.getlocal(level, i)
    if not n then break end
    if n == name then
      value = v
      found = true
    end
    i = i + 1
  end
  if found then
    return resolve_attrs()
  end

  -- try upvalues
  local func = debug.getinfo(level).func
  i = 1
  while true do
    local n, v = debug.getupvalue(func, i)
    if not n then break end
    if n == name then return v end
    i = i + 1
  end

  -- not found; get global
  value = getfenv(func)[name]
  return resolve_attrs()
end

local function capture_vars(level, __no_meta_table, __no_environment, __no_globals)
  level = level + 1
  -- captures all variables in scope which is
  --useful for evaling user input in the given stack frame
  local ar = debug.getinfo(level, "f")
  if not ar then return {}, '?', 0 end

  local vars = {__UPVALUES__={}, __LOCALS__={}}
  local i

  local func = ar.func
  if func then
    i = 1
    while true do
      local name, value = debug.getupvalue(func, i)
      if not name then break end
      --ignoring internal control variables
      if string.sub(name, 1, 1) ~= '(' then
        vars[name] = value
        vars.__UPVALUES__[i] = name
      end
      i = i + 1
    end
    if not __no_environment then
      vars.__ENVIRONMENT__ = getfenv(func)
    end
  end

  if not __no_globals then
    vars.__GLOBALS__ = getfenv(0)
  end

  i = 1
  while true do
    local name, value = debug.getlocal(level, i)
    if not name then break end
    if string.sub(name, 1, 1) ~= '(' then
      vars[name] = value
      vars.__LOCALS__[i] = name
    end
    i = i + 1
  end

  vars.__VARSLEVEL__ = level

  if __no_meta_table then
    return vars
  end

  if func then
    --Do not do this until finished filling the vars table
    setmetatable(vars, { __index = getfenv(func), __newindex = getfenv(func) })
  end

  --Do not read or write the vars table anymore else the metatable functions will get invoked!

  return vars

end

local function restore_vars(level, vars)

  local i
  local written_vars = {}

  i = 1
  while true do
    local name, value = debug.getlocal(level, i)
    if not name then break end
    if vars[name] and string.sub(name, 1, 1) ~= '(' then
      debug.setlocal(level, i, vars[name])
      written_vars[name] = true
    end
    i = i + 1
  end

  local ar = debug.getinfo(level, "f")
  if not ar then return end

  local func = ar.func
  if func then

    i = 1
    while true do
      local name, value = debug.getupvalue(func, i)
      if not name then break end
      if vars[name] and string.sub(name, 1, 1) ~= '(' then
        if not written_vars[name] then
          debug.setupvalue(func, i, vars[name])
        end
        written_vars[name] = true
      end
      i = i + 1
    end
  end
end

make_command('h', 'help', "", function(Debugger, file, line, topic)
  local _,command
  if topic and commands[topic] then
    command = commands[topic] or string.format('no help topic found for %s', topic)
    Debugger:write(command:help())
    return OPS.nop
  end
  for _,command in pairs(commands) do
    Debugger:write(command:help())
  end
  return OPS.nop
end)

make_command('w', 'where', "shows call stack", function(Debugger, file, line, args, lvl)
  if args == 'a' then
    lvl = 1
  else
    lvl = lvl + 1
  end
  Debugger:write(debug.traceback("", lvl))
  return OPS.nop
end)

make_command('l', 'list', "shows the current file", function(Debugger, file, line)
  Debugger:show(input, file, line)
  return OPS.nop
end)

make_command('q', 'quit', "", function(Debugger)
  return OPS.q
end)

make_command('c', 'continue', "go until the next breakpoint", function(Debugger)
  return OPS.c
end)

make_command('n', 'next', "step once", function(Debugger)
  Debugger:advance(STATE_OVER, 1, 0)
  return OPS.c
end)

make_command('o', 'out [number of frames]', "step out of the existing function", function(Debugger)
  Debugger:advance(STATE_OUT, 0, -1)
  return OPS.c
end)

make_command('s', 'step', "step down a frame", function(Debugger)
  Debugger:advance(STATE_IN, 0, 1)
  return OPS.c
end)

make_command('v', 'variables', "list all reachable variables and thier values", function(Debugger, file, line, topic, lvl)
  local vars = capture_vars(lvl + 1)
  for key, val in pairs(vars) do
    Debugger:write(key..':\n\n')
    Debugger:dump(val)
    Debugger:write('\n\n')
  end
  return OPS.nop
end)

make_command('lb', 'list breakpoints', "", function(Debugger)
  for file, lines in pairs(Debugger.breaks) do
    for line, is_set in pairs(lines) do
      local str = string.format("%s:%s (%s)\n", file, line, tostring(is_set))
      Debugger:write(str)
    end
  end
  return OPS.nop
end)
make_command('sb', 'set breakpoint [file:line]', "", function(Debugger, file, line, args)
  file,line = unpack(args:split(':'))
  line = tonumber(line)
  if file and line then
    Debugger:set_breakpoint(file, line)
  end
  return OPS.nop
end)
make_command('db', 'delete breakpoint [file:line]', "", function(Debugger, file, line, args)
  file,line = unpack(args:split(':'))
  line = tonumber(line)
  if file and line then
    Debugger:remove_breakpoint(file, line)
  end
  return OPS.nop
end)
make_command('t', 'trace', [[Show every function call.
The trace is buffered to a table.  Calling trace again will dump it.]], function(Debugger)
  if Debugger.trace then
    Debugger.trace = false
    Debugger:dump_trace()
  else
    Debugger.trace = true
  end
  return OPS.c
end)

make_command('x', 'eval', [[evals the statment via loadstring in the current strack frame
This function can't set local variables in the stack because loadstring returns a function.  Any input that
doesn't match an op defaults to eval.]], function(Debugger, file, line, eval, lvl)
  local function reply(msg)
    Debugger:write(msg .. '\n')
    return OPS.nop
  end

  local ok, func = pcall(loadstring, eval)
  if not ok and not func then
    return reply("Compile error: "..func)
  end
  if not func then
    eval = 'return ' .. eval
    ok, func = pcall(loadstring, eval)
    if not (ok and func) then
      return reply("Loadstring returns a function, try using the return statement.")
    end
  end

  local vars = capture_vars(lvl+1)

  setfenv(func, vars)
  local isgood, res = pcall(func)

  if not isgood then
    return reply("Run error: "..res)
  end
  restore_vars(lvl+1, vars)

  local msg = utils.dump(res)
  return reply(msg)
end)

function Debugger:initialize(io)
  self.io = io
  self.lvl = 0
  self.event = ""
  self.previous_break_hash = nil
  self.op = OPS.nop
  self.state = STATE_DEFAULT
  self.steps = 0
  self.breaks = {}
  self.stack_target = nil
  self.hooked = false
  self.trace = false
  self.traces = {}
end

function Debugger:read()
  return self.io.stdin:read('*l')
end

function Debugger:write(...)
  return self.io.write(...)
end

function Debugger:dump_trace()
  self:write(table.concat(self.traces, ""))
  self.traces = {}
end

function Debugger:dump(...)
  local string_table = {}
  for k,v in pairs({...}) do
    table.insert(string_table, utils.dump(v))
  end
  return self:write('\n\n', table.concat(string_table, '\t'), '\n')
end

function Debugger:open(...)
  return self.io.open(...)
end

function Debugger:advance(state, steps, relative_stack_target)
  self.state = state
  self.steps = steps or 0
  if relative_stack_target ~= nil then
    self.stack_target = self.lvl + relative_stack_target
  end
end

function Debugger:set_hook()
  if self.hooked then
    return
  end
  self.hooked = true
  local Debugger = self
  debug.sethook(function(...)
    Debugger:hook(...)
  end, "crl")
end

function Debugger:set_breakpoint(file, line)
  if not self.hooked then
    self:set_hook()
  end
  self.breaks[file] = self.breaks[file] or {}
  self.breaks[file][line] = true
end

function Debugger:remove_breakpoint(file, line)
  if self.breaks[file] then
    self.breaks[file][line] = nil
  end
end

--[[
shows lines around current break
]]--
function Debugger:show(input, file, line)
  local before = 10
  local after = 10
  line = tonumber(line or 1)

  if not string.find(file,'%.') then file = file..'.lua' end

  local f = self:open(file,'r')
  if not f then
    -- looks for a file in the package path
    local path = package.path or LUA_PATH or ''
    for c in string.gmatch (path, "[^;]+") do
      local c = string.gsub (c, "%?%.lua", file)
      f = self:open(c,'r')
      if f then
        break
      end
    end

    if not f then
      self:write('Cannot find '..file..'\n')
      return
    end
  end

  local i = 0
  for l in f:lines() do
    i = i + 1
    if i >= (line - before) then
      if i > (line + after) then break end
      if i == line then
        self:write('*** ' ..i ..'\t'..l..'\n')
      else
        self:write('    '..i.. '\t'.. l..'\n')
      end
    end
  end
  f:close()
end


function Debugger:has_breakpoint(file, line)
  return self.breaks[file] and self.breaks[file][line]
end

function Debugger:input(file, line, event, lvl)
  local ok, msg, op
  if file and line then
    self:write(string.format('\nbreak at %s:%s (%s)', file, line, event))
  end
  self:write("\n> ")
  input = self:read()

  ok, op = pcall(self.process_input, self, file, line, input, lvl+2)

  if not ok then
    self:write('ERROR: call failed', op)
    return OPS.q
  end
    -- use last op if no input
  if not op then
    op = self.op
  end

  return op
end

function Debugger:process_input(file, line, input, lvl)
  if not input or #input == 0 then
    self:write('Give me something.')
    return OPS.nop
  end

  local op, args
  -- 1 or 2 byte command
  if #input == 1 or #input == 2 then
    op = input
  -- 1 byte comand with args
  elseif input:sub(2, 2) == ' ' then
    op = input:sub(1, 1)
    args = input:sub(3)
  -- 2 byte comand with args
  elseif input:sub(3, 3) == " " then
    op = input:sub(0, 2)
    args = input:sub(4)
  end

  -- if the op doesn't exist, eval the expression and hope for the best
  if not op or not commands[op] then
    op = 'x'
    args = input
  end

  -- lets not put this into every command
  if args and #args == 0 then
    args = nil
  end

  return commands[op]:call(self, file, line, args, lvl)
end

function Debugger:should_break(file, line, event)

  if event == "call" then
    if self.state == STATE_IN and self.stack_target == self.lvl then
      self:advance(STATE_OVER)
    end
    return false
  end

  if event == "return" then
    if self.state == STATE_OUT and self.stack_target == self.lvl then
      self:advance(STATE_OVER)
    end
    return false
  end

  -- only line events at this point
  if self:has_breakpoint(file, line) then
    self:advance(STATE_OVER, 1, 0)
    return false
  end

  if self.state == STATE_OVER and self.lvl <= self.stack_target then
    -- have we arrived?
    if self.steps > 1 then
      self.steps = self.steps - 1
      return false
    end
    self:advance(STATE_DEFAULT)
    return true
  end

  return false

end

function Debugger:calculate_break_hash(event, file, line)
  -- a little hack to ignore accidentally leaking debugger lvl events which
  -- happen when we set new breakpoints with debugger() (ie, we step into the debugger)
  -- the stack offset introspection causes a problem with this
  return string.format('%s-%s-%s', event, file, line)
end

function Debugger:hook(event, line)
  local lvl, total_depth, file, current_hash
  -- NOTE: this hook is called via two paths - one via a Lua debug hook event, and one when we set a breakpoint

  -- lvl is the how deep the is at this point
  -- self.lvl = depth beneath the Debugger's entry stack frame
  lvl, self.lvl = unpack(get_lvl(1))
  file, line = unpack(getinfo(lvl+1))

  if self.trace then
    table.insert(self.traces, fmt("%s:%s\n", file, line))
  end

  -- NOTE: dragons are here- should_break has side effects
  if not self:should_break(file, line, event) then
    return
  end

  local current_hash = self:calculate_break_hash(event, file, line)
  -- make ourselves reentrant
  if self.previous_break_hash and self.previous_break_hash == current_hash then
    return
  end
  self.previous_break_hash = current_hash

  self.op = self:input(file, line, event, lvl+1)

  while true do
    if self.op == OPS.q then
      return debug.sethook()
    elseif self.op == OPS.c then
      return
    elseif self.op == OPS.nop then
      self.op = self:input(file, line, event, lvl+1)
    else
      process.stdout:write('Unrecognized command: ' .. self.op .. '\n')
      self.op = OPS.nop
    end
  end
end

local function dump_lua()
  -- useful function for debugging - dumps all variables in all frames
  local JSON = require('json')
  local stack = {}
  local _, lvl = unpack(get_lvl(1))
  local function dump(o)
    return utils.dump(o, 0, true)
  end
  for i=2,lvl do
    local vars = capture_vars(i, true, true, true)
    vars.__LOCALS__ = nil
    vars.__UPVALUES__ = nil
    vars.__VARSLEVEL__ = nil
    table.insert(stack, dump(vars))
  end

  local lua_dump = {}
  lua_dump.stack = stack
  lua_dump.tb = debug.traceback("", 2)
  virgo["config"]["monitoring_token"] = "******"
  lua_dump.virgo = dump(virgo)
  lua_dump.globals = dump(getfenv(0))
  return JSON.stringify(lua_dump)
end

local function install(io)
  if not io then
    error('Debugger needs an io')
  end
  debugger = Debugger:new(io)
  return debugger_entry
end

return {
  dump_lua = dump_lua,
  install = install
}
