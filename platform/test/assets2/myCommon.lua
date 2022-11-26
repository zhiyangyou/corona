    

function SafeRemoveGroup(g)
    if g ~= nil then
        g:removeSelf();
    end
end

function SafeRemoveObject(g)
    if g ~= nil then
        g:removeSelf();
    end
end

function DisplayRemoveSafeRemoveGroup(g)
    if g ~= nil then
        display.remove(g)
        g = nil
    end

end

function tablePrint(node)
    local cache, stack, output = {},{},{}
    local depth = 1
    local output_str = "{\n"

    while true do
        local size = 0
        for k,v in pairs(node) do
            size = size + 1
        end

        local cur_index = 1
        for k,v in pairs(node) do
            if (cache[node] == nil) or (cur_index >= cache[node]) then

                if (string.find(output_str,"}",output_str:len())) then
                    output_str = output_str .. ",\n"
                elseif not (string.find(output_str,"\n",output_str:len())) then
                    output_str = output_str .. "\n"
                end

                -- This is necessary for working with HUGE tables otherwise we run out of memory using concat on huge strings
                table.insert(output,output_str)
                output_str = ""

                local key
                if (type(k) == "number" or type(k) == "boolean") then
                    key = "["..tostring(k).."]"
                else
                    key = "['"..tostring(k).."']"
                end

                if (type(v) == "number" or type(v) == "boolean") then
                    output_str = output_str .. string.rep('\t',depth) .. key .. " = "..tostring(v)
                elseif (type(v) == "table") then
                    output_str = output_str .. string.rep('\t',depth) .. key .. " = \n"..string.rep('\t',depth).."{\n"
                    table.insert(stack,node)
                    table.insert(stack,v)
                    cache[node] = cur_index+1
                    break
                else
                    output_str = output_str .. string.rep('\t',depth) .. key .. " = '"..tostring(v).."'"
                end

                if (cur_index == size) then
                    output_str = output_str .. "\n" .. string.rep('\t',depth-1) .. "}"
                else
                    output_str = output_str .. ","
                end
            else
                -- close the table
                if (cur_index == size) then
                    output_str = output_str .. "\n" .. string.rep('\t',depth-1) .. "}"
                end
            end

            cur_index = cur_index + 1
        end

        if (size == 0) then
            output_str = output_str .. "\n" .. string.rep('\t',depth-1) .. "}"
        end

        if (#stack > 0) then
            node = stack[#stack]
            stack[#stack] = nil
            depth = cache[node] == nil and depth + 1 or depth - 1
        else
            break
        end
    end

    -- This is necessary for working with HUGE tables otherwise we run out of memory using concat on huge strings
    table.insert(output,output_str)
    output_str = table.concat(output)

    print(output_str)
end
function similarEqual(num1, num2)
    local minus =  math.abs((num1 - num2));

    return math.abs((num1 - num2)) < 0.05
end

function arrTable_similarEqual(t1, t2)
    print(#t1)
    print(#t2)
    assert(#t1 == #t2)
    for i = 1, #t1, 1 do
        assert(similarEqual(t1[i], t2[i]))
    end
end

function table_similarEqual(t1, t2)

    for key, value in pairs(t1) do

        if type(t2[key])  == 'table' then
            local ret = table_similarEqual(t1[key], t2[key]);
            if ret==false then
                return false ;
            end
        elseif type(t2[key]) == 'number' then
           
            local ret = similarEqual(t1[key], t2[key]);
            if ret == false then
                return false 
            end
        end
    end
    return true;

end

function copyTable(ori_tab)
    if (type(ori_tab) ~= "table") then
        return nil;
    end
    local new_tab = {};
    for i,v in pairs(ori_tab) do
        local vtyp = type(v);
        if (vtyp == "table") then
            new_tab[i] = th_table_dup(v);
        elseif (vtyp == "thread") then
            -- TODO: dup or just point to?
            new_tab[i] = v;
        elseif (vtyp == "userdata") then
            -- TODO: dup or just point to?
            new_tab[i] = v;
        else
            new_tab[i] = v;
        end
    end
    return new_tab;
end

--[[释放GroupObject的内存
	将group想象为一棵树，其中的DisplayObject则是分叉节点
	此方法递归删除子元素
	当level为0，则保留group本身；否则也删除group(默认)
--]]
local flag
function cleanGroups(curGroup, level, flag)
    local level = level or 1
    if curGroup then
        if curGroup.numChildren and curGroup.isCppSpine==nil  then -- 有这个属性，则说明curGroup是一个GroupObject
            --while curGroup.numChildren > 0 do
            -- 删除儿子节点
            for i = curGroup.numChildren, 1, -1 do
                cleanGroups(curGroup[i], level + 1, flag)
            end
            if level > 0 then
                if (curGroup) then
                    display.remove(curGroup)
                    curGroup._isRemove = true
                    curGroup = nil
                end
            end
        else -- 若是DisplayObject，直接删除并返回
            if curGroup ~= nil then
                display.remove(curGroup)
                curGroup._isRemove = true
                curGroup = nil
            end
        end
    end
    return curGroup
end

function newTouchableTxt(parentGroup, posX, posY, str, callBack,fontSize)
    if fontSize==nil then
        fontSize = 30 
    end
    local txt = display.newText(str, posX, posY, native.systemFont, fontSize)
    txt.anchorX = 0
    txt.anchorY = 0

    function txt:touch(event)
        if event.phase == 'began' then
            callBack(txt, event)
            return true
        end
    end

    parentGroup:insert(txt);
    txt:addEventListener('touch', txt)
    return txt;
end

function getTetxureUseNumber()
    return system.getInfo("textureMemoryUsed")
end

function getTextureUseStr()
    return  "textureMemoryUsed: "..  tostring( getTetxureUseNumber()/1024/1024)..' MB'
end



function createFPSMonitor(parentGroup)
    local fpsCount = 0
    local deltaCount = 0
    local FPS_TOTAL = 40
    local localGroup = display.newGroup()
    parentGroup:insert(localGroup)
    localGroup.x = -200
    localGroup.y = 0
    local fps_text = display.newText('FPS:', 0, 0, native.systemFont, 24)
    fps_text.size = 40
    local lasttime = 0
    Runtime:addEventListener(
        'enterFrame',
        function(event)
            fpsCount = fpsCount + 1
            local delta = event.time - lasttime
            if fpsCount > FPS_TOTAL then
                -- fps_text.text = "FPS:   " .. tostring(math.floor( event.time  ))
                fps_text.text = 'FPS:   ' .. tostring(math.floor(1000 / (deltaCount / FPS_TOTAL)))
                deltaCount = 0
                fpsCount = 0
            else
                deltaCount = deltaCount + delta
            end

            -- fps_text:setReferencePoint(display.TopLeftReferencePoint)
            fps_text.anchorX = 0
            fps_text.anchorY = 0
            fps_text.x = 0
            fps_text.y = 20
            lasttime = event.time
        end
    )
    localGroup:insert(fps_text)

    return localGroup
end

function TableClear(t)
    if t == nil then
        return
    end

    for index, v in ipairs(t) do
        t[index] = nil
    end

    for i = 1, #t, 1 do
        table.remove(t, 1)
    end
    t = nil
end

--为lua加载骨骼动画
function loadSkeleton(atlasFile, jsonFile, skin, animation, x, y, scale)

    local imageLoader = function(path)
        print('path=' .. path)
        local paint = {
            type = 'image',
            filename = path
        }
        return paint
    end
    local function GetLastCharIndex(str, char)
        local lastindex = 0
        for i = 1, string.len(str) do
            if string.sub(str, i, i) == char then
                lastindex = i
            end
        end
        return lastindex
    end

    -- load the atlas
    local atlas =
    spine.TextureAtlas.new(
        spine.utils.readFile(atlasFile),
        function(imageName)
            local relativePath = string.find(atlasFile, '/', 3)
            local lastIndex = GetLastCharIndex(atlasFile, '/')
            local texturePath = string.sub(atlasFile, 0, lastIndex) .. imageName
            local paint = {
                type = 'image',
                filename = texturePath
            }
            return paint
        end
    )

    -- load the JSON and create a Skeleton from it
    local json = spine.SkeletonJson.new(spine.AtlasAttachmentLoader.new(atlas))
    json.scale = scale
    local skeletonData = json:readSkeletonDataFile(jsonFile)
    local skeleton = spine.Skeleton.new(skeletonData)
    skeleton.scaleY = -1 -- Corona's coordinate system has its y-axis point downwards
    skeleton.group.x = x
    skeleton.group.y = y

    -- Set the skin if we got one
    if skin then
        skeleton:setSkin(skin)
    end

    local animationStateData = spine.AnimationStateData.new(skeletonData)
    animationStateData.defaultMix = 0.5
    local animationState = spine.AnimationState.new(animationStateData)

    skeleton.group.isVisible = false

    skeleton.group.name = jsonFile

    animationState:setAnimationByName(0, animation, true)

    return {
        skeleton = skeleton,
        state = animationState
    }
end

function gridLayout2(leftTopPosX, leftTopPosY, width, height, wGridNum, hGridNum)

end

function gridLayout(leftTopPosX, leftTopPosY, width, height, wGridNum, hGridNum)

    local ret = {}

    local gridWidth  = width / wGridNum
    local gridHeight = height / hGridNum

    for rowIndex = 1, hGridNum, 1 do
        -- local rowData = {}
        for colIndex = 1, wGridNum, 1 do
            local pos =
            {
                leftTopPosX + (colIndex - 1) * gridWidth,
                leftTopPosY + (rowIndex - 1) * gridHeight
            }

            ret[(rowIndex - 1) * wGridNum + colIndex] = pos
        end

    end

    return ret


end

function getTime()
    return os.clock()*1000;
end

function DoSomethingCostTime(func)
    local t1 = getTime();
    func();
    local t2 = getTime();
    return t2 - t1;
end
