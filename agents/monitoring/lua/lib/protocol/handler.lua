local Object = require('core').Object
local logging = require('logging')

local systemInfo  = require('../system_info')

local Handler = Object:extend()

function Handler:initialize(protocol, conn)
  self._protocol = protocol
  self._conn = conn

  self._protocol:on('message', function(msg)
    logging.log(logging.DEBUG, 'Got request: ' .. msg.method)

    if msg.method == 'system_info.network_interfaces' then
      return self:getNetworkInterfaces()
    else
      logging.log(logging.DEBUG, 'No request handler for ' .. msg.method)
    end
  end)
end

function Handler:getNetworkInterfaces()
  return systemInfo.getNetworkInterfacesInfo()
end

local exports = {}
exports.Handler = Handler
return Handler
