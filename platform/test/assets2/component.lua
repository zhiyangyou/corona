local M = {}
local widget = require( "widget" )
local graphics = require("graphics")
local common = require("common")

function M.createBtn(parent, name, width, x, y, func)
    local btnWidth = width or 100
    local options = {
        width = btnWidth,
        height = 20,
        numFrames = 1,
        sheetContentWidth = btnWidth,
        sheetContentHeight = 20
    }
    local buttonSheet = graphics.newImageSheet( "data/btn22.png", options )

    local button = widget.newButton(
            {
                sheet  = buttonSheet,
                defaultFrame = 1,
                label = "Default",
                labelColor = { default={ 1, 1, 1 }, over={ 0.5, 0.5, 0.5 } },
                onEvent = function(event)
                    if ( "ended" == event.phase ) then
                        if func then
                            func()
                        end
                    end
                end
            }
    )
    button.x = x
    button.y = y or 20
    button:setLabel(name)
    button:scale(0.8, 1)
    parent:insert(button)

    return button
end

function M.createNewSlider(parent, listener)
    local function sliderListener( event )
        if listener then
            listener(event)
        end
    end

    local slider = widget.newSlider(
            {
                x = display.contentCenterX,
                y = display.contentCenterY + 120,
                width = 400,
                value = 0,
                listener = sliderListener
            }
    )
    parent:insert(slider)

    return slider
end

function M.createText(str, x, y, parent)
    local myText = display.newText( str, x, y, native.systemFont, 16 )
    myText:setFillColor( 0, 0, 0 )
    if parent then
        parent:insert(myText)
    end
    return myText
end

local sortCfg = {
    ["sortStyle_1"] = {
        imgBgPath = "data/image166.png",
        sortBtnPath = "data/image167.png",
        sortIconPath = "data/listPoint.png"
    },
}

function M.createSortList(params)
    local gParent = params.gParent
    local x = params.x or 0
    local y = params.y or 0
    local cfg = params.cfg or {}
    local listener = params.listener
    local curSortIndex = params.curSortIndex			--当前选中页签
    local sortCfgKey = params.sortCfgKey or "sortStyle_1"
    local cellWidth = params.cellWidth or 229
    local cellHeight = params.cellHeight or 35
    local changeYScale = params.changeYScale
    local hideButtonBottom = params.hideButtonBottom
    local isShowSortBtn = params.isShowSortBtn or true
    local isStopClick = params.isStopClick
    local sortBtnBgWidth = params.sortBtnBgWidth or 42

    local bgGroup = display.newGroup()
    local listGroup = display.newGroup()
    local gGroup = display.newGroup()
    gGroup:insert(bgGroup)
    gGroup:insert(listGroup)
    if common.isExist(gParent) then
        gParent:insert(gGroup)
    end

    common.setXY( gGroup, x, y)
    if changeYScale then
        gGroup.yScale = -gGroup.yScale
    end

    --伴侣排序
    local imgBg = common.createRegularNinePatchImage{
        imgPath = sortCfg[sortCfgKey].imgBgPath,
        width = cellWidth,
        height = cellHeight,
        cornerWidth = 5,
        cornerHeight = 5
    }
    imgBg.alpha = 0.2
    bgGroup:insert( imgBg )
    common.touchDelegate{
        target = imgBg,
        onRelease = nil,
    }

    --按钮
    local sortGroup = display.newGroup()
    gGroup:insert(sortGroup)

    local sortBtn = common.createRegularNinePatchImage{
        imgPath = sortCfg[sortCfgKey].sortBtnPath,
        width = sortBtnBgWidth,
        height = cellHeight,
        cornerWidth = 5,
        cornerHeight = 5
    }
    common.setXY(sortBtn, imgBg.x + imgBg.width * .5 - sortBtn.width * .5, imgBg.y)
    sortGroup:insert(sortBtn)
    sortBtn.isVisible = not hideButtonBottom
    --排序按钮图标
    local sortIcon = display.newImageRect(sortCfg[sortCfgKey].sortIconPath, 42, 35 )
    sortGroup:insert(sortIcon)
    common.setXY(sortIcon, sortBtn.x, sortBtn.y)
    if sortCfg[sortCfgKey].yScale then
        sortIcon.yScale = sortCfg[sortCfgKey].yScale
    end

    if isShowSortBtn then
        if #cfg == 1 then
            sortBtn.isVisible = false
            sortIcon.isVisible = false
        end
    end

    local default_X = imgBg.x - imgBg.width * .5
    local default_Y = imgBg.y
    local moveState = 0
    local topHeight = cellHeight * #cfg
    local sortList = {}
    local state	= -1	-- -1:打关闭列表、1:打开列表

    --列表移动的默认速度，height 和 Y 变换速度的比例
    local defaultSpeed, percent = 5, .5

    local enterFrameListener, onStateChange
    -- 取消帧监听
    local function removeEnterFrame()
        Runtime:removeEventListener("enterFrame", enterFrameListener)
    end

    local function isStopChange(index, isUp)
        if isUp == 1 then
            if sortList[index].y == -cellHeight * (index - 1) then
                return true

            end
            if sortList[index].y -(-cellHeight * (index - 1)) < defaultSpeed then
                sortList[index].y = -cellHeight * (index - 1)
                return true
            end
        else
            if sortList[index].y == 0 then
                return true

            end
            if -sortList[index].y < defaultSpeed then
                sortList[index].y = 0
                return true
            end
            return false
        end

    end


    -- 改变高度和Y值
    local function changeHeight(itemObject)
        if not common.isExist(itemObject) then
            removeEnterFrame()
            return
        end
        local isStop = false
        local dValue = -1
        if state == 1 then
            isStop = itemObject.height >= topHeight
            if topHeight - itemObject.height < defaultSpeed then
                dValue = topHeight - itemObject.height
            end
        else
            isStop = itemObject.height <= cellHeight
            if  itemObject.height - cellHeight < defaultSpeed then
                dValue = itemObject.height - cellHeight
            end
        end

        if not common.isExist(itemObject) or isStop then
            removeEnterFrame()
            itemObject:resetHeight( state == 1 and topHeight or cellHeight)
            itemObject.y = state == 1 and -topHeight * .5 + cellHeight * .5 or 0

            moveState = 0
            return
        else
            local curSpeed = dValue ~= -1 and dValue or defaultSpeed
            itemObject:resetHeight(itemObject.height + curSpeed * state)
            itemObject.y = itemObject.y - curSpeed * percent * state


            for i = 1, #sortList do
                if not isStopChange(i, state) then
                    sortList[i].y = sortList[i].y - defaultSpeed  * state
                end
            end
        end
    end

    -- 帧监听方法
    enterFrameListener = function(event)
        changeHeight(imgBg)
    end

    -- 添加帧监听
    local function addEnterFrame()
        removeEnterFrame()
        Runtime:addEventListener("enterFrame", enterFrameListener)
    end

    --显示移动
    local  function showSortList()
        moveState = 1
        addEnterFrame()
    end

    --每次点击排序则更换一次列表顺序
    local function changList(index, isSort)
        if index then
            for i = 1, #cfg do
                sortList[i].textOption:setFillColor(0, 0, 0)
                sortList[i].split.isVisible = true
                if sortList[i].index == index then
                    local temp = sortList[i]:getInfo()
                    sortList[i]:setInfo(sortList[1]:getInfo())
                    sortList[1]:setInfo(temp)
                end
            end
        end
        --给选中排序添加属性
        sortList[1].textOption:setFillColor(0, 0, 1)
        sortList[1].split.isVisible = false
        if isSort then
            showSortList()
        end
    end

    local function changeSortScale()
        if #cfg > 1 then
            if sortCfg[sortCfgKey].yScale then
                sortIcon.yScale = state
            else
                sortIcon.yScale = -state
            end
        end
    end

    local function addTouch()
        common.touchDelegate{
            target=sortGroup,
            onRelease=function( )
                if moveState == 1 then				--print("如果列表正在滚动那么return-----------")
                    return
                end
                state = state == -1 and 1 or -1
                changeSortScale()

                showSortList()
                onStateChange()
            end
        }
    end

    onStateChange = function()
        sortGroup:removeTouch()
        local scaleTarget
        if state == 1 then
            scaleTarget = sortList[1].textOption
        else
            scaleTarget = gGroup
        end
        addTouch(scaleTarget)
    end

    local function createList(v)
        local index = v.index
        local sortGroup = display.newGroup()
        local rect = display.newRect(sortGroup, imgBg.x - imgBg.width * .5 + 5, imgBg.y, cellWidth - 10, cellHeight)
        common.setAnchor(rect, 0, .5)
        rect:setFillColor(0.8,0.8,0.8,1)

        --列表中的文本
        local textOption = M.createText(v.text, default_X + 13, default_Y, sortGroup)
        common.setAnchor(textOption, 0, .5)
        if changeYScale then
            textOption.yScale = -textOption.yScale
        end

        --下划线
        local split = display.newImageRect("data/split.png", 225, 1 )
        sortGroup:insert(split)
        common.setAnchor(split, .5, 1)
        common.setXY(split, imgBg.x, textOption.y + cellHeight * .5)

        sortGroup.textOption = textOption
        sortGroup.split = split
        sortGroup.index = index
        sortGroup.isVisible = true
        sortList[index] = sortGroup

        function sortGroup:getInfo()
            return { textOption = self.textOption.text, index = self.index }
        end

        function sortGroup:setInfo(params)
            self.textOption.text = params.textOption
            self.index = params.index
        end

        --文本点击事件
        common.touchDelegate{
            target = rect,
            scaleTarget = textOption,
            onRelease = function()
                sortGroup:selectClick()
            end,
        }

        function sortGroup:selectClick()
            if isStopClick and isStopClick(sortList[index].index) then
                return
            end
            if curSortIndex ~= sortList[index].index then
                curSortIndex = sortList[index].index
                state = -1
                changList(sortList[index].index, true)
                if listener then
                    listener(curSortIndex)
                    changeSortScale()
                end
                onStateChange()
            end
        end

        listGroup:insert(sortGroup)

        return sortGroup
    end

    local  function initList()
        if not next(cfg) then
            return
        end
        local tSortGroup = {}
        --初始化列表内容
        for i = #cfg, 1, -1 do
            local sortGroup = createList(cfg[i])
            tSortGroup[cfg[i].index] = sortGroup
        end

        changList(curSortIndex)
        function gGroup:selectClick(index)
            tSortGroup[index]:selectClick()
        end
    end

    addTouch()
    initList()
    return gGroup

end

return M