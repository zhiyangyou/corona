local spine = require 'spine'
    

local lastTime = 0
local swirl = spine.SwirlEffect.new(400)
local swirlTime = 0
display.setDefault('background', 0.2, 0.2, 0.2, 1)
local mainGroup = display.newGroup()
local TEST_COUNT = 1  
local testSpines = {}
local testCppSpines = {}
local useLua = false
local ShowIndex = 1
local pma = false
local function newTouchableTxt(posX, posY, str, callBack)
    local txt = display.newText(str, posX, posY, native.systemFont, 30)
    txt.anchorX = 0
    txt.anchorY = 0

    function txt:touch(event)
        if event.phase == 'began' then
            callBack(txt, event)
            return true
        end
    end

    txt:addEventListener('touch', txt)
    return txt ;
end

function createFPSMonitor()
    local fpsCount = 0
    local deltaCount = 0
    local FPS_TOTAL = 20
    local localGroup = display.newGroup()
    localGroup.x = -100
    localGroup.y = 700
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

createFPSMonitor()

local spinePaths = {
    
    {"data1/spineboy.atlas", "data1/spineboy-pro.json", "default", "death",130,150,0.4},
    {"data1/spineboy.atlas", "data1/spineboy-pro.json", "default", "hoverboard",130,250,0.4},
    {"data1/spineboy.atlas", "data1/spineboy-pro.json", "default", "idle",130,250,0.4},
    {"data1/spineboy.atlas", "data1/spineboy-pro.json", "default", "idle-turn",130,250,0.4},
    {"data1/spineboy.atlas", "data1/spineboy-pro.json", "default", "jump",130,250,0.4},
    {"data1/spineboy.atlas", "data1/spineboy-pro.json", "default", "portal",130,250,0.4},
    {"data1/spineboy.atlas", "data1/spineboy-pro.json", "default", "run",130,250,0.4},
    {"data1/spineboy.atlas", "data1/spineboy-pro.json", "default", "run-to-idle",130,250,0.4},
    {'data1/spineboy.atlas', 'data1/spineboy-pro.json', 'default', 'shoot', 130, 250, 0.4},
    {"data1/spineboy.atlas", "data1/spineboy-pro.json", "default", "walk",130,250,0.4},
    {'data1/tank.atlas', 'data1/tank-pro.json', 'default', 'drive', 180,250,0.1},
    {'data1/tank.atlas', 'data1/tank-pro.json', 'default', 'shoot', 180,250,0.1}, --- 掉帧
    {"data1/stretchyman.atlas", "data1/stretchyman-pro.json", 'default', 'sneak', 110, 250, 0.2},
    {'data1/coin.atlas', 'data1/coin-pro.json', 'default', 'animation', 110, 250, 0.8},
    {"data1/raptor.atlas", "data1/raptor-pro.json", 'default', "walk",130,250,0.2},
    {"data1/raptor.atlas", "data1/raptor-pro.json", 'default', "gun-grab",130,250,0.2},
    {"data1/raptor.atlas", "data1/raptor-pro.json", 'default', "gun-holster",130,250,0.2},
    {"data1/raptor.atlas", "data1/raptor-pro.json", 'default', "jump",130,250,0.2},
    {"data1/raptor.atlas", "data1/raptor-pro.json", 'default', "roar",130,250,0.2},
    {"data1/goblins.atlas", "data1/goblins-pro.json", "goblin", "walk",120,250,0.5},
    {"data1/goblins.atlas", "data1/goblins-pro.json", "goblingirl", "walk",120,250,0.5},
    {"data1/vine.atlas", "data1/vine-pro.json", 'default', "grow",120,250,0.3},
    
}

local luaSpinePaths = {}

--为lua加载骨骼动画
function loadSkeleton(atlasFile, jsonFile, skin, animation, x, y, scale)
    -- to load an atlas, we need to define a function that returns
    -- a Corona paint object. This allows you to resolve images
    -- however you see fit
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

local function showLuaSpine(index)
    if #testSpines > 0 then
        for i = 1, #testSpines, 1 do
            local s = testSpines[i]
            s.skeleton.group.isVisible = false
        end
    end
    testSpines = {}

    local curPath = spinePaths[index]
    for i = 1, TEST_COUNT, 1 do
        local luaSpineObject = loadSkeleton(curPath[1], curPath[2], curPath[3], curPath[4], curPath[5], curPath[6], 1.0)
        local Scale = curPath[7]
        luaSpineObject.skeleton.group.xScale = Scale
        luaSpineObject.skeleton.group.yScale = Scale
        testSpines[i] = luaSpineObject
        luaSpineObject.skeleton.group:translate(200+i ,-50+i);
    end
end

local function showCppSpine(index)
    if #testCppSpines > 0 then
        for i = 1, #testCppSpines, 1 do
            local s = testCppSpines[i]
            s.isVisible = false
            s:removeSelf()
        end
    end

    testCppSpines = {}

    local curPath = spinePaths[index]
    for i = 1, TEST_COUNT, 1 do
        --可选: system.DocumentsDirectory
        local mySpine_Cpp =
            display.newSpine(
            mainGroup,
            curPath[1],
            curPath[2],
            curPath[3],
            curPath[4],
            system.ResourceDirectory,
            curPath[5],
            curPath[6],
            pma
        )
        local Scale = curPath[7]
        mySpine_Cpp.xScale = Scale
        mySpine_Cpp.yScale = Scale
        testCppSpines[i] = mySpine_Cpp
        testCppSpines[i]:translate(200+i ,-50+i);
    end
end

--使用lua或者cpp加载所有骨骼动画
local function LoadSpines()
    if useLua then
        showLuaSpine(ShowIndex)
    else
        showCppSpine(ShowIndex)
    end
end
LoadSpines()

    

Runtime:addEventListener(
    'enterFrame',
    function(event)
        -- local temp =0 ;
        -- for i = 1, 10000000, 1 do
        --     temp  = temp+1 ;
        -- end

        local currentTime = event.time / 1000
        local delta = currentTime - lastTime
        lastTime = currentTime

        -- local c = collectgarbage("count")
        -- print("Memory:",c)

        swirlTime = swirlTime + delta
        local percent = swirlTime % 2
        if (percent > 1) then
            percent = 1 - (percent - 1)
        end
        swirl.angle = spine.Interpolation.apply(spine.Interpolation.pow2, -60, 60, percent)
        for i = 1, TEST_COUNT, 1 do
            local luaSpineObject = testSpines[i]
            if luaSpineObject ~= nil then
                local skeleton = luaSpineObject.skeleton
                local state = luaSpineObject.state
                skeleton.group.isVisible = true
                state:update(delta)
                state:apply(skeleton)
                skeleton:updateWorldTransform()
            end
        end
    end
)

local function RemoveAllSpines()
    if #testCppSpines > 0 then
        for i = 1, #testCppSpines, 1 do
            local s = testCppSpines[i]
            s.isVisible = false
            s:removeSelf()
        end
    end
    testCppSpines = {}

    if #testSpines > 0 then
        for i = 1, #testSpines, 1 do
            local s = testSpines[i]
            s.skeleton.group.isVisible = false
        end
    end
    testSpines = {}
end

newTouchableTxt(
    30,
    500,
    'useLua=' .. tostring(useLua),
    function(txt, e)
        if useLua then
            useLua = false
        else
            useLua = true
        end
        RemoveAllSpines()
        LoadSpines()
        txt.text = 'useLua=' .. tostring(useLua)
    end
)

newTouchableTxt(
    30,
    600,
    'showIndex=' .. tostring(ShowIndex),
    function(txt, e)
        ShowIndex = ShowIndex + 1
        if ShowIndex > #spinePaths then
            ShowIndex = 1
        end
        RemoveAllSpines()
        LoadSpines()
        txt.text = 'showIndex=' .. tostring(ShowIndex)
    end
)

local txtDebugInfo =  newTouchableTxt(
    -300,
    650,
    '' ,
    function(txt, e)
        if #testCppSpines > 0 then
            for i = 1, #testCppSpines, 1 do
                local s = testCppSpines[i]
                s:clearMeshCache();
            end
        end
    end
)



local ispause = false
 newTouchableTxt(
    30,
    550,
    'pause' ,
    function(txt, e)
        if ispause then 
            ispause = false
        else
            ispause =true ;
        end
        if #testCppSpines > 0 then
            for i = 1, #testCppSpines, 1 do
                local s = testCppSpines[i]
                if ispause then
                    s:pause();
                else
                    s:resume();
                end
            end
        end
    end
)


 newTouchableTxt(
    130,
    550,
    'pma='..tostring(pma),
    function(txt, e)
        if pma then 
            pma = false
        else
            pma =true 
        end
        txt.text ='pma='..tostring(pma)
        if #testCppSpines > 0 then
            for i = 1, #testCppSpines, 1 do
                local s = testCppSpines[i]
                s:setPremultipliedAlpha(pma);
            end
        end
    end
)

local closeClipper =false
newTouchableTxt(
    330,
    550,
    'closeClipper='..tostring(closeClipper),
    function(txt, e)
        if closeClipper then 
            closeClipper = false
        else
            closeClipper =true 
        end
        txt.text ='closeClipper='..tostring(closeClipper)
        if #testCppSpines > 0 then
            for i = 1, #testCppSpines, 1 do
                local s = testCppSpines[i]
                s:setClipperEnable(closeClipper);
            end
        end
    end
)



local defaultMode = 'default'
local wireFrameMode = 'wireframe'
local strCurMode = 'curMode:' .. defaultMode
local isWireFrameMode = false
newTouchableTxt(
    100,
    720,
    '模式',
    function(txt, event)
        local newStr = ''
        local curMode = ''
        if isWireFrameMode then
            isWireFrameMode = false
            newStr = '模式:' .. 'D'
            curMode = defaultMode
        else
            isWireFrameMode = true
            newStr = '模式:' .. 'W'
            curMode = wireFrameMode
        end
        txt.text = newStr
        display.setDrawMode(curMode)
        return true
    end
)
    

Runtime:addEventListener(
    'enterFrame',
    function(event)
        if #testCppSpines > 0 then
            local sp = testCppSpines[1];  
            local info =  sp.debugInfo;
            txtDebugInfo.text = string.format("click to release cache\ndebug:batch:%d save:%d attachMentNum:%d mesh:%d activeMesh:%d vert:%d cliper:%d texture:%d children:%d",
            info.batchNum,info.saveByBatchNum,info.visiableAttachmentNum,info.cacheMeshNum,info.activeMeshNum,info.vertNum,info.clipperNum,info.textureNum, info.childrenNum);
        end
    end
)

 
 

 