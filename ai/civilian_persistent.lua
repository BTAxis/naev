include("ai/civilian.lua")

function create ()
   mem.loiter = math.huge -- This is the amount of waypoints the pilot will pass through before leaving the system
   create_post()
end 