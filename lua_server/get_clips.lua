return function(req, res)

    -- fix: https://github.com/luvit/luvit/issues/1229
    os.setlocale('C', 'time')
    res:setHeader("Date", os.date("!%a, %d %b %Y %H:%M:%S GMT"))
    res:setHeader('Content-Type', 'application/octet-stream');
    res:setHeader('Access-Control-Allow-Origin', '*');
    res:setHeader('Access-Control-Expose-Headers', 'X-Json-Size');

    local url = req.url;
    local limit = 0;
    local count = 0;

    local quert_sep = url:find("?");
    if quert_sep == nil then
        return badReq(res);
    else
        local qs = querystring.parse(url:sub(quert_sep + 1, -1))
        limit = tonumber(qs.limit)
        count = tonumber(qs.count)

        if not count or not limit then
            return badReq(res);
        end
    end

    res.statusCode = 204;

    local clips, image_blobs = clip.get_clips(limit, count);
    if not clips then
        res:finish();
        return
    end

    res.statusCode = 200;

    local json = JSON.stringify(clips);
    res:setHeader('X-Json-Size', #json);
    res:write(json);

    if image_blobs then
        res:write( image_blobs);
    end

    res:finish();
end
