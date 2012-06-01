local Object = require('core').Object
local JSON = require('json')

--[[ Info ]]--
local Info = Object:extend()
function Info:initialize()
  self._s = sigar:new()
  self._params = {}
end

function Info:serialize()
  return {
    jsonPayload = JSON.stringify(self._params)
  }
end

local NilInfo = Info:extend()

--[[ CPUInfo ]]--
local CPUInfo = Info:extend()
function CPUInfo:initialize()
  Info.initialize(self)
  local cpus = self._s:cpus()
  self._params = {}
  for i=1, #cpus do
    local info = cpus[i]:info()
    local data = cpus[i]:data()
    local bucket = 'cpu.' .. i - 1

    self._params[bucket] = {}
    for k, v in pairs(info) do
      self._params[bucket][k] = v
    end
    for k, v in pairs(data) do
      self._params[bucket][k] = v
    end
  end
end

--[[ DiskInfo ]]--
local DiskInfo = Info:extend()
function DiskInfo:initialize()
  Info.initialize(self)
  local disks = self._s:disks()
  local name, usage
  for i=1, #disks do
    name = disks[i]:name()
    usage = disks[i]:usage()
    if usage then
      self._params[name] = {}
      for key, value in pairs(usage) do
        self._params[name][key] = value
      end
    end
  end
end

--[[ MemoryInfo ]]--
local MemoryInfo = Info:extend()
function MemoryInfo:initialize()
  Info.initialize(self)
  local meminfo = self._s:mem()
  for key, value in pairs(meminfo) do
    self._params[key] = value
  end
end

--[[ NetworkInfo ]]--
local NetworkInfo = Info:extend()
function NetworkInfo:initialize()
  Info.initialize(self)
  local netifs = self._s:netifs()
  for i=1,#netifs do
    self._params.netifs[i] = {}
    self._params.netifs[i].info = netifs[i]:info()
    self._params.netifs[i].usage = netifs[i]:usage()
  end
end

--[[ Factory ]]--
function create(infoType)
  if infoType == 'CPU' then
    return CPUInfo:new()
  elseif infoType == 'MEMORY' then
    return MemoryInfo:new()
  elseif infoType == 'NETWORK' then
    return NetworkInfo:new()
  elseif infoType == 'DISK' then
    return DiskInfo:new()
  end
  return NilInfo:new()
end

--[[ Exports ]]--
local info = {}
info.CPUInfo = CPUInfo
info.DiskInfo = DiskInfo
info.MemoryInfo = MemoryInfo
info.NetworkInfo = NetworkInfo
info.create = create
return info