--[[ sm-ssc compatibility helpers
sm-ssc changes quite a few things which would make various SM4 content break.
Also, certain things are deprecated/removed from sm-ssc (and sometimes SM4 too).
--]]

--[[ Actor ]]
function Actor:hidden(bHide)
	Warn("hidden is deprecated, use visible instead. (used on ".. self:GetName() ..")")
	self:visible(not bHide)
end

--[[ ActorScroller: all of these got renamed, so alias the lowercase ones if
things are going to look for them. ]]
ActorScroller.getsecondtodestination = ActorScroller.GetSecondsToDestination
ActorScroller.setsecondsperitem = ActorScroller.SetSecondsPerItem
ActorScroller.setnumsubdivisions = ActorScroller.SetNumSubdivisions
ActorScroller.scrollthroughallitems = ActorScroller.ScrollThroughAllItems
ActorScroller.scrollwithpadding = ActorScroller.ScrollWithPadding
ActorScroller.setfastcatchup = ActorScroller.SetFastCatchup

--[[ GameState ]]
--Aliases for old GAMESTATE timing functions.
--These have been converted to SongPosition, but most themes still use these old functions.

function GameState:GetSongBeat() return self:GetSongPosition():GetSongBeat() end
function GameState:GetSongBeatNoOffset() return self:GetSongPosition():GetSongBeatNoOffset() end
function GameState:GetSongBPS() return self:GetSongPosition():GetCurBPS() end
function GameState:GetSongDelay() return self:GetSongPosition():GetDelay() end
function GameState:GetSongFreeze() return self:GetSongPosition():GetFreeze() end
