require 'myCommon'
local composer = require 'composer'
display.setDefault('background', 0.2, 0.2, 0.2, 1)

local TestCaseGroups =
{

    { sceneName = "测试ETC2纹理", luaPath = "testDemos.testETC2Image" }
    -- { sceneName = "遍历spine", luaPath = "testDemos.iterateSpines" }
    -- { sceneName = "内存碎片化测试", luaPath = "testDemos.switchAnimations" },
    --  { sceneName = "单纹理-多纹理,内存对照", luaPath = "testDemos.testTextureMemoryUsage" },
    -- { sceneName = "utf-8路径加载测试", luaPath = "testDemos.u8PathSpine" },
    --  { sceneName = "测试用例-Api全覆盖", luaPath = "testDemos.ApiTestCase" },
    --  { sceneName = "BenchMark-基准测试", luaPath = "testDemos.spineBenchMark" },
    -- { sceneName = "描边字测试", luaPath = "testDemos.outlineText" },

}

local mainGroup = display.newGroup()
local curShowIndex = 1
local curShowInfo = nil;
local txtSwitch = nil;
local txtReloead = nil;

local function setSwitchLabelLabel(txt, bIsWait)
    if txt == nil then
        return;
    end
    if bIsWait == true then
        txt.text = "正在加载场景 Index:" .. tostring(curShowIndex) .. " " .. TestCaseGroups[curShowIndex].sceneName
    else

        txt.text = "场景 Index:" .. tostring(curShowIndex) .. " " .. TestCaseGroups[curShowIndex].sceneName
    end

end

local function ReloadCurScene()
    setSwitchLabelLabel(txtSwitch, true);
    local waitTime = 300
    if curShowInfo ~= nil then
        print("remove Scene " .. curShowInfo.luaPath)
        composer.removeScene(curShowInfo.luaPath)
    end
    print(string.format("等待%dms后开始加载", waitTime))
    timer.performWithDelay(waitTime, function(e)
        curShowInfo = TestCaseGroups[curShowIndex];
        composer.showOverlay(curShowInfo.luaPath);
        setSwitchLabelLabel(txtSwitch, false);
    end)

end

ReloadCurScene();





    

txtReloead = newTouchableTxt(mainGroup, -500, 150, "Reload  Scene",
    function(txt, e)
        ReloadCurScene();
    end)
-- mainGroup:insert(txtReloead);

local isShowWireFrame = false;
local txtSwitchDrawMode = newTouchableTxt(mainGroup, -500, 200, "SwitchDrawMode",
    function(txt, e)
        print(isShowWireFrame)
        if isShowWireFrame then
            isShowWireFrame = false
        else
            isShowWireFrame = true
        end
        if isShowWireFrame then
            display.setDrawMode("wireframe", true)
        else
            display.setDrawMode("default")
        end
    end)



txtSwitch = newTouchableTxt(mainGroup, -500, 250, "",
    function(txt, e)
        curShowIndex = curShowIndex + 1;
        if curShowIndex > #TestCaseGroups then
            curShowIndex = 1
        end
        setSwitchLabelLabel(txt, true)
        ReloadCurScene();
    end)
setSwitchLabelLabel(txtSwitch, true)
mainGroup:insert(txtSwitch);



local sampleCount = 200;
local sampleData ={}
local function resetSampleData(data)
    data.count = 0;

    data.fResourceCreateTime = 0
    data.fResourceUpdateTime = 0
    data.fResourceDestroyTime = 0
    data.fPreparationTime = 0
    data.fRenderTimeCPU = 0
    data.fRenderTimeGPU = 0
    data.fDrawCallCount = 0
    data.fTriangleCount = 0
    data.fLineCount = 0
    data.fGeometryBindCount = 0
    data.fProgramBindCount = 0
    data.fTextureBindCount = 0
    data.fUniformBindCount = 0
end

local function addSampleData(originData, addData)
    originData.count = originData.count + 1;
    originData.fResourceCreateTime = originData.fResourceCreateTime + addData.fResourceCreateTime
    originData.fResourceUpdateTime = originData.fResourceUpdateTime + addData.fResourceUpdateTime
    originData.fResourceDestroyTime = originData.fResourceDestroyTime + addData.fResourceDestroyTime
    originData.fPreparationTime = originData.fPreparationTime + addData.fPreparationTime
    originData.fRenderTimeCPU = originData.fRenderTimeCPU + addData.fRenderTimeCPU
    originData.fRenderTimeGPU = originData.fRenderTimeGPU + addData.fRenderTimeGPU
    originData.fDrawCallCount = originData.fDrawCallCount + addData.fDrawCallCount
    originData.fTriangleCount = originData.fTriangleCount + addData.fTriangleCount
    originData.fLineCount = originData.fLineCount + addData.fLineCount
    originData.fGeometryBindCount = originData.fGeometryBindCount + addData.fGeometryBindCount
    originData.fProgramBindCount = originData.fProgramBindCount + addData.fProgramBindCount
    originData.fTextureBindCount = originData.fTextureBindCount + addData.fTextureBindCount
    originData.fUniformBindCount = originData.fUniformBindCount + addData.fUniformBindCount
end

local function statisticDataToStr(data,count)
    local str = string.format(
        [[  
fResourceCreateTime     %.3f        
fResourceUpdateTime     %.3f       
fResourceDestroyTime    %.3f        
fPreparationTime            %.3f    
fRenderTimeCPU              %.3f  
fRenderTimeGPU        %.3f  
fDrawCallCount        %.0f  
fTriangleCount        %.0f  
fLineCount            %.0f 
fGeometryBindCount    %.0f      
fProgramBindCount     %.0f     
fTextureBindCount     %.0f     
fUniformBindCount     %.0f         
        ]]
,data.fResourceCreateTime   /count
,data.fResourceUpdateTime   /count
,data.fResourceDestroyTime  /count
,data.fPreparationTime      /count
,data.fRenderTimeCPU        /count
,data.fRenderTimeGPU        /count
,data.fDrawCallCount        /count
,data.fTriangleCount        /count
,data.fLineCount            /count
,data.fGeometryBindCount    /count
,data.fProgramBindCount     /count
,data.fTextureBindCount     /count
,data.fUniformBindCount     /count
);
    return str 
end
local count         = 0;
local txtFrameCount = newTouchableTxt(mainGroup, -500, 700, "FrameCount:" .. tostring(count),function(txt, e) end)
-- local txtStatistic = newTouchableTxt(mainGroup, 600, 0, "Statistic:" .. tostring(count),function(txt, e) end)
local function Callback_StatisticSample(data)
    if sampleData.count < sampleCount then
        addSampleData(sampleData,data);
    else    
        local str = statisticDataToStr(sampleData,sampleCount);
        -- txtStatistic.text = str 
        print(str)
        resetSampleData(sampleData)
    end
end

resetSampleData(sampleData);



createFPSMonitor(mainGroup)
local fps =0 ;

local timeTotal = 0 ;
local fpsSampleCount = 60;
local t1 = getTime();
local t2 = getTime();
Runtime:addEventListener("enterFrame", function(e)
     
    -- spinecpp.test1111();
    t2 =t1 ;
    t1 = getTime();
    timeTotal = timeTotal+( t1 - t2 );
    count = count + 1
    if  count%fpsSampleCount==0 then 
        fps = 1.0/(timeTotal/fpsSampleCount)
        -- fps = timeTotal
        timeTotal =0 ;
        txtFrameCount.text =   getTextureUseStr()
    
    end 
    

end)


local function handleLowMemory( event )
    print( "lua Memory warning received!" )
end
  
Runtime:addEventListener( "memoryWarning", handleLowMemory )
