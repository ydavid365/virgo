local spawn = require('childprocess').spawn
local constants = require('constants')
local misc = require('monitoring/default/util/misc')

function runner(name)
  return spawn('python', {'agents/monitoring/runner.py', name})
end

local child

local function start_server(callback)
  local data = ''
  callback = misc.fireOnce(callback)
  child = runner('server_fixture_blocking')
  child.stderr:on('data', function(d)
    p(d)
    callback(d)
  end)

  child.stdout:on('data', function(chunk)
    p(chunk)
    data = data .. chunk
    if data:find('TLS fixture server listening on port 50061') then
      callback()
    end
  end)
  return child
end

local function stop_server(child)
  if not child then return end
  pcall(function()
    child:kill(constants.SIGUSR1) -- USR1
  end)
end

process:on('exit', function()
  stop_server(child)
end)

process:on("error", function(e)
  stop_server(child)
end)

-- This will skip all the functions in an export list but still be able to call them individually
local function skip_all(exports, reason)
  for i,v in pairs(exports) do
    p("Setting a skip " .. i .. " for " .. reason)
    exports[i] = function(test, asserts)
      test.skip("Skipping " .. i .. " for " .. reason)
    end
  end
  return exports
end

local exports = {}
exports.runner = runner
exports.start_server = start_server
exports.stop_server = stop_server
exports.skip_all = skip_all
return exports
