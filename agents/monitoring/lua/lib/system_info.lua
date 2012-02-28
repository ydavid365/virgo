function getNetworkInterfacesInfo()
  local s = sigar:new()
  local netifs = s:netifs()
  local result = {}
  local i = 1

  while i <= #netifs do
    result[i] = netifs[i]:info()
    i = i + 1
  end

  return result
end

local exports = {}
exports.getNetworkInterfacesInfo = getNetworkInterfacesInfo
return exports
