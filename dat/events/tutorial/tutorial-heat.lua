-- This is the tutorial: heat.

include("dat/events/tutorial/tutorial-common.lua")
include("enum.lua")

-- localization stuff, translators would work here
lang = naev.lang()
if lang == "es" then
else -- default english
    title1 = "Tutorial: Heat"
    message1 = [[Welcome to the heat tutorial.

In this tutorial we will examine how heat accumulates in your ship, how it affects you, and what you can do to get rid of it.]]
   message2 = [[Heat is generated by outfits, particularly weapons. Each time you fire a weapon, some of the energy needed to fire it will be converted into heat. This heat accumulates in the weapon outfit, and if the outfit gets hotter and hotter, it starts to lose its efficiency. For weapons, this means that they become inaccurate when they get hot, and start firing more slowly when very hot.

You have been equipped with two Vulcan Cannons. To see how heat builds up in weapons, try firing into space (using %s) now. Keep an eye on your weapon indicators on the HUD - after a short while you should see the bars filling up red.]]
   message3 = [[As you probably saw, your weapons started shooting slightly off to the sides when they heated up. This can be very problematic in combat, so always keep an eye on your weapons' heat levels.

When weapons get hot, they will automatically cool because the heat flows from the outfits into the ship hull. So, it can be a good idea to not use all of your weapons at the same time if you expect a long fight. Switch between weapons to let them cool down periodically.]]
   message4 = [[So that's heat in a nutshell, right? As it turns out, there's more to it. We just learned that weapons cool down by transferring heat into the ship hull. But what if the ship hull gets hot?

One thing that happens is that the ship becomes more visible to enemies. Objects that are hotter than the space around them will stand out more. So if your ship is hot, you might attract more attention.

Another thing that happens is that your outfits, which are connected to the hull, start to cool more slowly! After all, heat transfer depends heavily on the difference in temperature between objects. So, if your ship gets very hot, you may find that your weapons don't cool as fast anymore, or not at all in extreme cases.

For the purpose of this tutorial, we have made your ship rather hot. Notice the ship temperature indicator on the HUD, and notice also how your weapons aren't cooling anymore.]]
   message5 = [[As you can see, having a hot ship is very inconvenient. It would be nice if ships could lose their heat somehow, wouldn't it? Fortunately, there are some ways to lose heat.

First of all, ships radiate off heat if they are hotter than the space around them. This happens automatically and continuously, but it is a relatively slow process, especially for larger ships. It may be possible to accelerate the rate of heat loss by installing outfits to that effect.

Heat is also reset to the base level when you land. So every time you take off, you will start out with a nice, cool hull.

There is a third way to lose heat, however.]]
   message6 = [[Every ship has the ability to go into a special cooling mode called Active Cooldown. When in Active Cooldown, ships rapidly lose their heat. By the end of the cooldown cycle, the ship and all its outfits will have cooled to the galactic base temperature.

Let's try this now. Enter Active Cooldown mode by pressing %s. If you are moving, you will automatically brake to a stop.]]
   message7 = [[You are now in Active Cooldown mode. You will see a progress bar above your ship. It is slowly draining. When the bar is completely empty, your ship will have cooled down. Don't touch any of the controls, or Active Cooldown will be canceled and you will have to restart it.
   
While you are in active cooldown, your energy and shields will not recharge. So try to use it only in a safe spot.

Wait for your ship to cool. Keep a close eye on the temperature of your ship!]]
   message8 = [[Well done! You have completed the Active Cooldown cycle, and your ship is as cool as the space around it.

As you might have noticed during the cooldown process, your heat didn't drop very quickly until the cooling process was almost over. This is because heat is lost at an exponential rate in Active Cooldown mode. It starts off very slow, but picks up a lot of speed over time. What this means is that when using Active Cooldown, you're going to want to wait all the way until the end. Active Cooldown always takes a minimum amount of time, and if you abort the cooldown period, you have to start over from the beginning next time!

Another important thing to remember is that Active Cooldown only works if the ship is sitting still. You can't do it on the move! So while it might seem attractive to use Active Cooldown in a fight to cool your weapons, you make yourself very vulnerable by doing so.]]
   message9 = [[You now know about the important heat mechanics in the game. As a final tip, remember that the heat rules apply to other ships just as much as they do to you. Dodging an enemy until his weapons overheat is a valid tactic!

Congratulations! This concludes the heat tutorial.]]

   wepomsg = [[Use %s to fire your weapons (%ds remaining)]]
   waitomsg = [[Observe your ship's heat (%ds remaining)]]
   coolomsg = [[Enter Active Cooldown (%s)]]
end

function create()
    -- Set up the player here.
    player.teleport("Cherokee")
    pilot.clear()
    pilot.toggleSpawn(false) -- To prevent NPCs from getting targeted for now.
    system.get("Mohawk"):setKnown(false)

    pp = player.pilot()
    pp:setPos(vec2.new(0, 2000))
    player.swapShip("Vendetta", "Vendetta", "Paul 2", true, true)
    pp:rmOutfit("all")
    pp:addOutfit("Milspec Orion 2301 Core System", 1, true)
    pp:addOutfit("Nexus Dart 300 Engine", 1, true)
    pp:addOutfit("S&K Light Combat Plating", 1, true)
    pp:setEnergy(100)
    pp:setHealth(100, 100)
    pp:addOutfit("Vulcan Gun", 2)
    pp:setDir(90)
    player.msgClear()

    pp:setNoLand()
    pp:setNoJump()

    tk.msg(title1, message1)
    tk.msg(title1, message2:format(tutGetKey("primary")))
    
    stages = enumerate("weapons", "wait", "coolwait", "cool")
    stage = stages.weapons

    flytime = 10 -- seconds of fly time
    omsg = player.omsgAdd(wepomsg:format(tutGetKey("primary"), flytime), 0)
    
    -- We want to set the weapon heat just before the overheat point, for demonstration purposes.
    pp:setTemp(500)
    pp:setTemp(0, false)
    
    hook.timer(1000, "flyUpdate")
end

-- Make the player fire their weapons.
function flyUpdate()
   flytime = flytime - 1
   if stage == stages.weapons then
      player.omsgChange(omsg, wepomsg:format(tutGetKey("primary"), flytime), 0)
      if flytime < 0 then
         player.omsgRm(omsg)
         tk.msg(title1, message3)
         tk.msg(title1, message4)
         flytime = 5
         stage = stages.wait
         pp:setTemp(750)
         omsg = player.omsgAdd(waitomsg:format(flytime), 0)
      end
   elseif stage == stages.wait then
      player.omsgChange(omsg, waitomsg:format(flytime), 0)
      if flytime < 0 then
         player.omsgRm(omsg)
         tk.msg(title1, message5)
         tk.msg(title1, message6:format(tutGetKey("cooldown")))
         stage = stages.coolwait
         omsg = player.omsgAdd(coolomsg:format(tutGetKey("cooldown")), 0)
         inputhook = hook.input("input")
      end
   elseif stage == stages.cool then
      if pp:temp() == 250 then
         tk.msg(title1, message8)
         tk.msg(title1, message9)
         cleanup()
      end
   end
   hook.timer(1000, "flyUpdate")
end

--Input hook
function input(inputname, inputpress)
   if inputname == "cooldown" then
      player.omsgRm(omsg)
      hook.timer(2000, "cooldownPressed")
      hook.rm(inputhook)
      stage = stages.cool
   end
end

--Timer called function.
function cooldownPressed()
   tk.msg(title1, message7)
end

-- Cleanup function. Should be the exit point for the module in all cases.
function cleanup()
    if not (omsg == nil) then player.omsgRm(omsg) end
    naev.keyEnableAll()
    naev.eventStart("Tutorial")
    evt.finish(true)
end
