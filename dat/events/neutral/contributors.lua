--[[
-- Event that spawns contributors as ships or NPCs in Zied.
--]]

include "scripts/fleethelper.lua"

lang = naev.lang()
if lang == "es" then
else -- default english
   -- These are the contributors that will show up as ships flying around in Zied.
   -- Elements consist of tables containing the contributor name or nick, and the message for them to say.
   contributors_ship = {{"Viruk", "Hello, I am Viruk. I made a good number of ship models for the game, including all of the Soromid and Za'lek ones."}
                       }

   -- These are the contributors that will show up as bar NPCs that show up on Bon Sebb and Sixbat.
   -- Elements consist of tables containing the contributor name or nick, and the message for them to say.
   contributors_bar = {{"xrogaan", "Python Python Python Python Python Python Python Python Python Python Python Python Python."}
                      }
end

function create()
   hook.jumpout("cleanup")
   hook.land("land")
   hook.load("land")
   hook.enter("enter")
   evt.save()
   enter()
end

-- Handle contributors-as-ships here.
function enter()
   -- The below is a list of all ships in the game (not special ships). Regenerate this when new ships are added.
   ships = {"Llama", "Hyena", "Schroedinger", "Gawain", "Drone", "Koala", "Quicksilver", "Rhino", "Pirate Rhino", "Mule", "Proteron Derivative", "Sirius Fidelity", "Soromid Brigand", "Shark", "Pirate Shark", "Empire Shark", "Soromid Reaver", "Lancelot", "Empire Lancelot", "Vendetta", "Dvaered Vendetta", "Pirate Vendetta", "Sirius Shaman", "Soromid Marauder", "Ancestor", "Dvaered Ancestor", "Pirate Ancestor", "Phalanx", "Dvaered Phalanx", "Pirate Phalanx", "Sirius Preacher", "Soromid Odium", "Admonisher", "Pirate Admonisher", "Empire Admonisher", "Vigilance", "Dvaered Vigilance", "Proteron Kahan", "Soromid Nyx", "Pacifier", "Empire Pacifier", "Kestrel", "Pirate Kestrel", "Proteron Archimedes", "Soromid Ira", "Hawking", "Empire Hawking", "Soromid Arx", "Sirius Dogma", "Sirius Divinity", "Goddard", "Dvaered Goddard", "Empire Peacemaker"}
   
   -- Iterate over the table of contributors and spawn the pilots.
   for _, con in ipairs(contributors_ship) do
      -- Select a random ship and spawn it.
      conship = addRawShips(ships[rnd.rnd(1, #ships)], "civilian_persistent", nil, "Naev Contributors")[1]
      conship:rename(con[1])
      conship:setInvincible(true)
      conship:setHilight(true)
      conship:setVisplayer(true)
      hook.pilot(conship, "hail", "hail", con[2])
   end
end

-- Makes the contributor say his text when hailed.
function hail(con, msg)
   player.commClose()
   tk.msg(con:name(), msg)
end

-- Handle contributors-as-bar-NPCs here.
function land()
   npcs = {}
   -- Iterate over the table of contributors and spawn the NPCs.
   for _, con in ipairs(contributors_bar) do
      local npcdata = {name = con[1], msg = con[2]} 
      id = evt.npcAdd("talk", con[1], "unknown", "One of the contributors to Naev is here in the bar.", 1)
      npcs[id] = npcdata 
   end
end

-- Makes the contributor say his text when approached.
function talk(id)
   local npcdata = npcs[id]
   tk.msg(npcdata.name, npcdata.msg)
end

function cleanup()
   evt.finish()
end