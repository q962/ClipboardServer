return function(req, res)

    local chunks = {}
    local length = 0
    local content = ""

    local content_type = req.headers['Content-Type'];
    if not content_type or not content_type:startswith("multipart/form-data;") then
        return badReq(res);
    end

    local boundary = content_type:split("=");
    if not boundary or boundary:len() ~= 2 then
        return badReq(res);
    end
    boundary = '--' .. boundary[2];

    req:on('data', function(chunk)

        chunks[length] = chunk

        content = content .. chunk;
    end)

    req:on('end', function()

        local quit = false;
        --[[ 结构是:
            form-data; {
                data_name: [
                    {
                        data_type: string,
                        data: blob
                        ...: ...
                    }
                ]
            }
        ]]
        local entitys = {};

        local stream_seek = 1;
        local match_count = 0;

        local function parse_entity(stream, entity_begin, entity_end, out_entity)
            local header = {};

            local entity_body_begin = 0

            local stream_p = entity_begin
            while true do
                local item_end = stream:find("\r\n", stream_p, true)
                local item_content = stream:sub(stream_p, item_end - 1);

                if #item_content == 0 then
                    entity_body_begin = item_end + 2
                    break
                end

                local item_split_val = item_content:split(": ");
                local item_key, item_value = item_split_val[1], item_split_val[2]
                header[item_key] = item_value

                stream_p = item_end + 2
            end

            local entity = {};
            entity.disposition = {}

            entity.content_type = header['Content-Type']

            local content_disposition = header['Content-Disposition']

            local part1 = content_disposition:split("; ")
            for i, v in ipairs(part1) do
                local v_s = v:split("=")
                if not v_s or #v_s <= 1 then
                    goto content
                end

                local key = v_s[1]
                local value = v_s[2]
                entity.disposition[key] = value:sub(2, #value - 1)

                ::content::
            end

            entity.content = stream:sub(entity_body_begin, entity_end);

            return entity.disposition["name"], entity
        end

        while true do
            -- do
            if stream_seek > #content then
                badReq(res)
            end

            local entity_boundary_begin, entity_boundary_end = content:find(boundary, stream_seek, true);

            stream_seek = entity_boundary_end + 1;

            if content:sub(entity_boundary_end + 1, entity_boundary_end + 1 + 1) == '--' then
                break
            elseif content:sub(entity_boundary_end + 1, entity_boundary_end + 1 + 1) == '\r\n' then
                stream_seek = stream_seek + 2;

                local entity_end = content:find(boundary, stream_seek, true);
                if not entity_end then
                    break
                end
                entity_end = entity_end - 2;

                local entity_name, content = parse_entity(content, stream_seek, entity_end - 1)

                entitys[entity_name] = entitys[entity_name] or {};
                table.insert(entitys[entity_name], content)

                stream_seek = entity_end + 2
            end
        end

        local image_entity = entitys["image"]
        local text_entity = entitys["text"]

        if image_entity then
            clip.push_image(image_entity[1].content)
        elseif text_entity then
            clip.push_text(text_entity[1].content)
        else
            return badReq(res);
        end

        req200(res)
    end)

end
