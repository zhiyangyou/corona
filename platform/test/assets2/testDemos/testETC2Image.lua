local composer = require("composer")

require "myCommon"
require 'math'
local sceneGroup = nil

    
local scene = composer.newScene()

function scene:create(event)

    sceneGroup = self.view

end

    
 
    
function scene.OnUpdate(e)

end
    
function scene:showJpgImage(gParent,useETC,imageName, w,h,x,y)

    local etcImageInfo =
    {
        path = string.format("etcTest/%s.ktx",imageName),
        enable = 1
    }
    local normalImageInfo = 
    {
        path = string.format("etcTest/%s",imageName),
        enable = 0
    }

    local useImage = nil 
    if useETC then 
        useImage = etcImageInfo
    else 
        useImage = normalImageInfo
    end
    print(useImage.path)
    local image =  display.newImage(gParent,useImage.path,useImage.enable) ;
    print("1111111111 complete"..useImage.path)
    if image ~= nil then 
        image.width = w ;
        image.height = h ;
        image.x  =     x ;
        image.y  =     y ;
    end 
end



function scene:reloadImages()
    if sceneGroup.imageGroup~=nil  then 
        sceneGroup.imageGroup:removeSelf();
    end

    sceneGroup.imageGroup = display.newGroup();
    sceneGroup:insert(sceneGroup.imageGroup);
    local etcY1 = 300 ;
    local etcY2 = 300 ;
    local normalY1 = 650 ;
    local normalY2 = 650 ;

    -- normalY1 = etcY1;
    -- normalY2 = etcY2;
    -- sceneGroup.imageShowMode ==0 or 
    if    sceneGroup.imageShowMode ==0 or  sceneGroup.imageShowMode ==1 then   
        
        self:showJpgImage(sceneGroup.imageGroup,true,"bg-4.jpg",500,300,200,etcY1)
        -- self:showJpgImage(sceneGroup.imageGroup,true,"minister-non4.png",500,300,750,etcY2)
        self:showJpgImage(sceneGroup.imageGroup,true,"king_2_5.png",500,300,750,etcY2)
    end
    if    sceneGroup.imageShowMode ==0 or  sceneGroup.imageShowMode ==2 then   
        self:showJpgImage(sceneGroup.imageGroup,false,"bg-4.jpg",500,300,200,normalY2)
        -- self:showJpgImage(sceneGroup.imageGroup,false,"minister-non4.png",500,300,750,normalY1)
        self:showJpgImage(sceneGroup.imageGroup,false,"king_2_5.png",500,300,750,normalY1)
    end

    
    
end 


function scene:show(event)

    sceneGroup = self.view
    local phase = event.phase
    sceneGroup.imageShowMode = 0 ;
    sceneGroup.imageShowModeCount = 0 ;
    if (phase == "will") then
        scene:reloadImages();
        local txtReloead = newTouchableTxt(sceneGroup, -400, 450, "reloadImages"..tostring( sceneGroup.imageShowMode),
        function(txt, e)
            sceneGroup.imageShowModeCount  = sceneGroup.imageShowModeCount+ 1;
            sceneGroup.imageShowMode = math.fmod(  sceneGroup.imageShowModeCount , 3)
            scene:reloadImages();
            txt.text = "reloadImages" ..tostring( sceneGroup.imageShowMode)
        end)

    elseif (phase == "did") then
    end
end

function scene:hide(event)

    local sceneGroup = self.view
    local phase = event.phase

    if (phase == "will") then


    elseif (phase == "did") then


    end
end

-- destroy()
function scene:destroy(event)

    local sceneGroup = self.view
    Runtime:removeEventListener('enterFrame', scene.OnShowTxtMemoryShow)
    Runtime:removeEventListener('enterFrame', scene.OnUpdate)
    sceneGroup:removeSelf();
end

-- -----------------------------------------------------------------------------------
-- Scene event function listeners
-- -----------------------------------------------------------------------------------
scene:addEventListener("create", scene)
scene:addEventListener("show", scene)
scene:addEventListener("hide", scene)
scene:addEventListener("destroy", scene)
-- -----------------------------------------------------------------------------------

return scene
