-- https://github.com/luvit/luvit/pull/858#issuecomment-166110522
-- fix luvit require quirks
_G._require = require
setfenv(1, setmetatable({}, {
    __newindex = _G,
    __index = _G
}))
require = _G._require
-----------------------------------------

require('pl')
stringx.import()

http = require("http")
net = require("net")
utils = require("utils")
table = require("table")
querystring = require('querystring');
JSON = require('json')
URL = require('url')
uv = require('uv')
fs = require('fs')
mime = require('./mime')

clip = require("gtkclip");

function ServerError(res)
    res.statusCode = 500;
    res:setHeader('Content-Type', 'text/plain');
    res:finish('server error');
end

function badReq(res)
    res.statusCode = 400;
    res:setHeader('Content-Type', 'text/plain');
    res:finish('Bad Request');
end

function req200(res)
    res:writeHead(200, {
        ["Content-Length"] = 0
    })
    res:finish()
end

local routes = {};

local function match_route(req, res)

    local req_path = req.url:split("?")[1]

    for path, opt in pairs(routes) do
        if req_path:find(path) ~= nil and req.method:lower() == opt.method:lower() and type(opt.handler) == "function" then
            local err, errmsg = pcall(opt.handler, req, res)
            if err == false then
                print(errmsg)
            end
            return true
        end
    end
end

local function route(opt)
    routes[opt.path] = opt;
end

route({
    method = 'POST',
    path = '^/api/push',
    handler = require("./push_clip")
})

route({
    method = 'GET',
    path = '^/api/get_clips',
    options = {
        ["Access-Control-Allow-Origin"] = 1
    },
    handler = require("./get_clips")
})

route({
    method = 'GET',
    path = '^/web',
    handler = function(req, res)

        local url = URL.parse(req.url)
        local path = url.pathname

        if path == "/web" or path == "/web/" then
            path = "/index.html"
        else
            path = path:sub(5)
        end

        local file_path = clip.web_root() .. path;
        fs.stat(file_path, function(err, stat)
            if err then
                res.statusCode = 404;
                return res:finish()
            end

            if stat.type ~= 'file' then
                res.statusCode = 404;
                return res:finish()
            end

            res:writeHead(200, {
                ["Content-Type"] = mime.getType(file_path),
                ["Content-Length"] = stat.size
            })

            fs.createReadStream(file_path):pipe(res)
        end)
    end
})

--[[
    实现 http 下载功能
    分段下载
]]
route({
    method = "GET",
    path = "^/api/download",
    handler = function(req, res)
    end
});

local server;

local function stop_server(server)
    if server then
        server:destroy()
    end
end

function clip.startServer(alias, ip, port)
    ip = options.ip or ip
    port = tonumber(port)
    alias = options.alias or alias

    if options.port and tonumber(options.port) >= 0 then
        port = options.port
    end

    stop_server(server)
    server = http.createServer(function(req, res)
        if not match_route(req, res) then
            badReq(res);
        end
    end):listen(tonumber(port), ip)

    local addr_info, error_msg, error_code = server:address()
    if addr_info == nil then
        clip.error(error_msg, error_code)
        return false;
    end

    clip.port(addr_info.port, ip, alias)
    return true;
end

-- 保持 uv 存活
local timer = uv.new_timer()
timer:start(10 * 1000, 10 * 1000, function()
end)

uv.run()

return 0;
