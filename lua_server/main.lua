local _module = module;
-- fix module path has containing dots
table.insert(package.loaders, function(name)
    name = name:gsub("%.", "/")
    local data = _module:resolve(name)
    if data then
        return load(data)
    end
end)

pcall(require, "./local-env")

_G.process = require('process').globalProcess()

_G.options = {}
for i=1, #args do
    local arg = args[i]

    if arg == "--ip" then
        options.ip = args[i+1]
        i = i+1
    elseif arg == "--alias" then
        options.alias = args[i+1]
        i = i+1
    elseif arg == "-p" then
        options.port = tonumber(args[i+1])
        i = i+1
    elseif arg == "--quit" then
        options.quit = true
    end
end

require('./clipserver')
