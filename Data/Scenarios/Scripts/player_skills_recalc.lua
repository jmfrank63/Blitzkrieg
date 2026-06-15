function RecalcSkills(so1,so2,so3,so4,so5,so6, pm1,pm2,pm3,pm4,pm5,pm6,pm7,pm8,pm9,pm10,pm11,pm12, p1,p2,p3,p4,p5,p6,p7,p8,p9,p10,p11,p12)
-- -------------parameters ------------------
-- p1- enemy destroyed (number)
-- p2 - enemy machinery captured
-- p3 - units lost unrecoverable
-- p4 - units returned
-- p5 - resources used
-- p6 - aviation called
-- p7 - houses destroyed
-- p8 - units level up
-- p9 - objectives completed
-- p10 - objective failed
-- p11 - time elapsed
-- p12 - game loaded

-- -------- skills -------------
-- s1 - tactics
-- s2 - logistics
-- s3 - carefulness(assault)
-- s4 - training
-- s5 - art of war
-- s6 - duty
----------------------------- variables-----------------------------------
local x = 0
local diff = 0
local d = 0
local po1 = p1 - pm1
local po2 = p2 - pm2
local po3 = p3 - pm3
local po4 = p4 - pm4
local po5 = p5 - pm5
local po6 = p6 - pm6
local po7 = p7 - pm7
local po8 = p8 - pm8
local po9 = p9 - pm9
local po10 = p10 - pm10
local po11 = p11 - pm11
local po12 = p12 - pm12

----------------------------- TACTICS -------------
----------------------------- formulas ------------
local	camp_t = p1/(p3+1)
local	miss_t = pm1/(pm3+1)
local	oldc_t = po1/(po3+1)
local	s1 = 0.4

	d = max(miss_t, camp_t)

	if (d == 0) then
		diff   = 0
	else
		diff = (miss_t - camp_t)/d
	end

	if (diff >= 0) then
		s1 = max(so1 + diff*(1 - so1)*0.7, 0.4)
	else	
		s1 = max(so1 + diff*(so1 - 0.4)*0.4, 0.4)
	end
----------------------------- LOGISTICS -------------
----------------------------- formulas ------------

local	camp_l = (p5+p6+p2)/(p11+1)
local	miss_l = (pm5+pm6+pm2)/(pm11+1)
local	oldc_l = (po5+po6+po2)/(po11+1)
local	s2 = 0.4

	d = max(miss_l, camp_l)

	if (d == 0) then
		diff   = 0
	else
		diff = (miss_l - camp_l)/d
	end

	if (diff >= 0) then
		s2 = max(so2 + diff*(1 - so2)*0.7, 0.4)
	else
		s2 = max(so2 + diff*(so2 - 0.4)*0.4, 0.4)
	end
----------------------------- CAREFULNES -------------
----------------------------- formulas ------------------------------------
local	camp_c = p7/(p3+1)
local	miss_c = pm7/(pm3+1)
local	oldc_c = po7/(po3+1)
local	s3 = 0.4

	d = max(miss_c, camp_c)

	if (d == 0) then
		diff   = 0
	else
		diff = (miss_c - camp_c)/d
	end

	if (diff >= 0) then
		s3 = max(so3 + diff*(1 - so3)*0.7, 0.4)
	else
		s3 = max(so3 + diff*(so3 - 0.4)*0.4, 0.4)
	end
----------------------------- STAFF -------------
---------------------formulas------------------------------------
local	camp_s = p8/(p3+1)
local	miss_s = pm8/(pm3+1)
local	oldc_s = po8/(po3+1)
local	s4 = 0.4

	d = max(miss_s, camp_s)

	if (d == 0) then
		diff   = 0
	else
		diff = (miss_s - camp_s)/d
	end

	if (diff >= 0) then
		s4 = max(so4 + diff*(1 - so4)*0.7, 0.4)
	else
		s4 = max(so4 + diff*(so4 - 0.4)*0.4, 0.4)
	end
----------------------------- ARTOFWAR -------------
---------------------formulas------------------------------------
local	camp_a = p11/(p12 + 1)
local	miss_a = pm11/(pm12 + 1)
local	oldc_a = po11/(po12 + 1)
local	s5= 0.4

	d = max(miss_a, camp_a)

	if (d == 0) then
		diff   = 0
	else
		diff = (miss_a - camp_a)/d
	end

	if (diff >= 0) then
		s5 = max(so5 + diff*(1 - so5)*0.7, 0.4)
	else
		s5 = max(so5 + diff*(so5 - 0.4)*0.4, 0.4)
	end
----------------------------- DUTY -------------
---------------------formulas------------------------------------
--local	camp_d = p9/(p11 + p10 + 1)
--local	miss_d = pm9/(pm11 + pm10 + 1)
--local	oldc_d = po9/(po11 + po10 + 1)
--local	s6 = 0.4

--	d = max(miss_d, camp_d)
--
--	if (d == 0) then
--		diff   = 0
--	else
--		diff = (miss_d - camp_d)/d
--	end
--
--	if (diff >= 0) then
--		s6 = max(so6 + diff*(1 - so6)*0.7, 0.4)
--	else
--		s6 = max(so6 + diff*(so6 - 0.4)*0.05, 0.4)
--	end

local	camp_d = p9/(p10 + 1)
local	miss_d = pm9/(pm10 + 1)
local	oldc_d = po9/(po10 + 1)
local	s6 = 0.4

	diff = p9-p10

	if (diff >= 0) then
		s6 = max(so6 + diff*(1 - so6)*0.7, 0.4)
	else
		s6 = max(so6 + diff*(so6 - 0.4)*0.4, 0.4)
	end


------------------------------------------------------------------
	return s1,s2,s3,s4,s5,s6
end;

function max(a, b)

	if (a >= b) then
		return a
	else
		return b
	end;
end;